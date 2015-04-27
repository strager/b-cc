#include <B/Error.h>
#include <B/Memory.h>
#include <B/Private/Assertions.h>
#include <B/Private/Config.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/Private/Mutex.h>
#include <B/Private/Queue.h>
#include <B/Process.h>

#include <stddef.h>
#include <stdio.h>

#if B_CONFIG_POSIX_PROCESS
# include <errno.h>
# include <sys/wait.h>
#endif

#if B_CONFIG_POSIX_SPAWN
# include <errno.h>
# include <signal.h>
# include <spawn.h>
#endif

struct B_ProcessInfo_ {
  B_ProcessExitCallback *callback;
  void *callback_opaque;

  B_ProcessID pid;
  B_OPTIONAL void *handle;

  // If this ProcessInfo is owned by a ProcessController,
  // link is locked by the owning ProcessController's lock.
  // Otherwise, link is unlocked.
  B_LIST_ENTRY(B_ProcessInfo_) link;

  // Used only when this ProcessInfo is *not* owned by a
  // ProcessController.
  struct B_ProcessExitStatus exit_status;
};

typedef B_LIST_HEAD(, B_ProcessInfo_) B_ProcessInfoList_;

struct B_ProcessController {
  struct B_Mutex lock;

  // Locked by lock.
  B_ProcessInfoList_ processes;
};

struct B_ProcessManager {
  struct B_ProcessControllerDelegate *delegate;
  struct B_ProcessController controller;
};

// Checks if the ProcessInfo's process has died. If so:
//
// 1. Remove the ProcessInfo from the owning
//    ProcessController list.
// 2. Add the ProcessInfo to exited_processes.
// 3. Update ProcessInfo::exit_status.
//
// Assumes the lock of the owning ProcessController is held.
static B_WUR B_FUNC bool
process_manager_check_locked_(
    B_BORROW struct B_ProcessInfo_ *,
    B_BORROW B_ProcessInfoList_ *exited_processes,
    B_OUT struct B_Error *);

#if B_CONFIG_POSIX_PROCESS
static struct B_ProcessExitStatus
exit_status_from_waitpid_status_(
    int waitpid_status) {
  // N.B. We can't use initializers here because older
  // compilers (like GCC 4.4) don't support initializers
  // with anonymous struct fields.
  if (WIFEXITED(waitpid_status)) {
    struct B_ProcessExitStatus exit_status;
    exit_status.type = B_PROCESS_EXIT_STATUS_CODE;
    exit_status.code.exit_code
      = WEXITSTATUS(waitpid_status);
    return exit_status;
  } else if (WIFSIGNALED(waitpid_status)) {
    struct B_ProcessExitStatus exit_status;
    exit_status.type = B_PROCESS_EXIT_STATUS_SIGNAL;
    exit_status.signal.signal_number
      = WTERMSIG(waitpid_status);
    return exit_status;
  } else {
    B_ASSERT(!WIFSTOPPED(waitpid_status));
    B_BUG();
  }
}
#endif

#if B_CONFIG_POSIX_SIGNALS
# if B_CONFIG_BUGGY_SIGFILLSET
static B_FUNC void
b_sigemptyset(
    sigset_t *set) {
  int rc = sigemptyset(set);
  B_ASSERT(rc == 0);
}
# endif

# if B_CONFIG_PEDANTIC_POSIX_SPAWN_SIGNALS
static B_FUNC void
b_sigaddset(
    sigset_t *set,
    int signal_number) {
  int rc = sigaddset(set, signal_number);
  B_ASSERT(rc == 0);
}

static B_FUNC void
b_sigdelset(
    sigset_t *set,
    int signal_number) {
  int rc = sigdelset(set, signal_number);
  B_ASSERT(rc == 0);
}
#endif

static B_FUNC void
b_sigfillset(
    sigset_t *set) {
# if B_CONFIG_BUGGY_SIGFILLSET
  b_sigemptyset(set);
  for (int i = 1; i < _NSIG; ++i) {
    b_sigaddset(set, i);
  }
# else
  int rc = sigfillset(set);
  B_ASSERT(rc == 0);
# endif
}
#endif

static struct B_ProcessManager *
b_controller_manager_(
    struct B_ProcessController *controller) {
  B_PRECONDITION(controller);

  return (void *) (((char *) controller)
    - offsetof(struct B_ProcessManager, controller));
}

B_WUR B_EXPORT_FUNC bool
b_process_manager_allocate(
    struct B_ProcessControllerDelegate *delegate,
    B_OUT_TRANSFER struct B_ProcessManager **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(delegate);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  if (!(delegate->register_process_id
      || delegate->register_process_handle)) {
    *e = (struct B_Error) {.posix_error = EINVAL};
    return false;
  }
  if (!!delegate->register_process_id
      != !!delegate->unregister_process_id) {
    *e = (struct B_Error) {.posix_error = EINVAL};
    return false;
  }
  if (!!delegate->register_process_handle
      != !!delegate->unregister_process_handle) {
    *e = (struct B_Error) {.posix_error = EINVAL};
    return false;
  }

  struct B_ProcessManager *manager = NULL;
  bool mutex_initialized = false;

  if (!b_allocate(
      sizeof(*manager), (void **) &manager, e)) {
    manager = NULL;
    goto fail;
  }
  *manager = (struct B_ProcessManager) {
    .delegate = delegate,
    .controller = {
      // .lock
      .processes = B_LIST_HEAD_INITIALIZER(
        &manager->controller.processes),
    },
  };
  if (!b_mutex_initialize(&manager->controller.lock, e)) {
    goto fail;
  }
  mutex_initialized = true;

  *out = manager;
  return true;

fail:
  if (mutex_initialized) {
    B_ASSERT(manager);
    (void) b_mutex_destroy(
      &manager->controller.lock, &(struct B_Error) {});
  }
  if (manager) {
    b_deallocate(manager);
  }
  return false;
}

B_WUR B_EXPORT_FUNC bool
b_process_manager_deallocate(
    B_TRANSFER struct B_ProcessManager *manager,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(manager);
  B_OUT_PARAMETER(e);

  struct B_ProcessInfo_ *process_info;
  struct B_ProcessInfo_ *process_info_tmp;
  B_LIST_FOREACH_SAFE(
      process_info,
      &manager->controller.processes,
      link,
      process_info_tmp) {
    // FIXME(strager): Do we call the callback?  (Yes.)
    // How?  (No idea.)
    fprintf(
      stderr,
      "Process found in deallocating ProcessManager; "
      "not yet implemented\n");
    B_LIST_REMOVE(process_info, link);
    b_deallocate(process_info);
  }
  b_deallocate(manager);
  return true;
}

static B_WUR B_FUNC bool
process_manager_check_locked_(
    B_BORROW struct B_ProcessInfo_ *process_info,
    B_BORROW B_ProcessInfoList_ *exited_processes,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(process_info);
  B_PRECONDITION(exited_processes);
  B_OUT_PARAMETER(e);

#if B_CONFIG_POSIX_PROCESS
  pid_t pid = process_info->pid;

  int status;
  pid_t rc = waitpid(pid, &status, WNOHANG);
  if (rc == 0) {
    // Process has not exited.
    return true;
  } else if (rc == pid) {
    process_info->exit_status
      = exit_status_from_waitpid_status_(status);
    B_LIST_REMOVE(process_info, link);
    B_LIST_INSERT_HEAD(
      exited_processes, process_info, link);
    return true;
  } else if (rc == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  } else {
    B_BUG();
  }
#else
# error "Unknown process implementation"
#endif
}

B_WUR B_EXPORT_FUNC bool
b_process_manager_check(
    B_BORROW struct B_ProcessManager *manager,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(manager);
  B_OUT_PARAMETER(e);

  bool ok = true;
  struct B_ProcessInfo_ *process_info;
  struct B_ProcessInfo_ *process_info_tmp;

  B_ProcessInfoList_ exited_processes
    = B_LIST_HEAD_INITIALIZER(&exited_processes);

  b_mutex_lock(&manager->controller.lock);
  B_LIST_FOREACH_SAFE(
      process_info,
      &manager->controller.processes,
      link,
      process_info_tmp) {
    if (!process_manager_check_locked_(
        process_info, &exited_processes, e)) {
      ok = false;
      break;
    }
  }
  b_mutex_unlock(&manager->controller.lock);

  B_LIST_FOREACH_SAFE(
      process_info,
      &exited_processes,
      link,
      process_info_tmp) {
    struct B_ProcessExitStatus *exit_status
      = &process_info->exit_status;
    switch (exit_status->type) {
    case B_PROCESS_EXIT_STATUS_CODE:
      fprintf(
        stderr,
        "Process %ld exited with code %ld\n",
        (long) process_info->pid,
        (long) exit_status->code.exit_code);
      break;
    case B_PROCESS_EXIT_STATUS_EXCEPTION:
      fprintf(
        stderr,
        "Process %ld exited with exception %#08X\n",
        (long) process_info->pid,
        (unsigned) exit_status->exception.code);
      break;
    case B_PROCESS_EXIT_STATUS_SIGNAL:
      fprintf(
        stderr,
        "Process %ld exited with signal %d\n",
        (long) process_info->pid,
        (int) exit_status->signal.signal_number);
      break;
    default:
      B_BUG();
    }

    // FIXME(strager)
    (void) process_info->callback(
      &process_info->exit_status,
      process_info->callback_opaque,
      &(struct B_Error) {});
    b_deallocate(process_info);
  }

  return ok;
}

B_WUR B_EXPORT_FUNC bool
b_process_manager_get_controller(
    B_BORROW struct B_ProcessManager *manager,
    B_OUT_BORROW struct B_ProcessController **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(manager);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *out = &manager->controller;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_process_controller_register_process_id(
    B_BORROW struct B_ProcessController *controller,
    B_ProcessID process_id,
    B_TRANSFER B_ProcessExitCallback *callback,
    B_TRANSFER void *callback_opaque,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(controller);
  B_PRECONDITION(process_id > 0);
  B_PRECONDITION(callback);
  B_OUT_PARAMETER(e);

  struct B_ProcessManager *manager
    = b_controller_manager_(controller);
  if (!manager->delegate->register_process_id) {
    *e = (struct B_Error) {.posix_error = ENOTSUP};
    return false;
  }

  struct B_ProcessInfo_ *process_info = NULL;
  bool process_info_in_list = false;

  if (!b_allocate(
      sizeof(*process_info), (void **) &process_info, e)) {
    process_info = NULL;
    goto fail;
  }
  *process_info = (struct B_ProcessInfo_) {
    .callback = callback,
    .callback_opaque = callback_opaque,
    .pid = process_id,
    .handle = NULL,
    // .link
    // .exit_status
  };

  // We must insert into our list first in case
  // ProcessControllerDelegate::register_process_id calls
  // e.g. b_process_manager_check.
  // TODO(strager): Write a test.
  b_mutex_lock(&controller->lock);
  B_LIST_INSERT_HEAD(
    &controller->processes, process_info, link);
  b_mutex_unlock(&controller->lock);
  process_info_in_list = true;

  if (!manager->delegate->register_process_id(
      manager->delegate, process_id, e)) {
    goto fail;
  }

  return true;

fail:
  if (process_info_in_list) {
    B_ASSERT(process_info);
    b_mutex_lock(&controller->lock);
    B_LIST_REMOVE(process_info, link);
    b_mutex_unlock(&controller->lock);
  }
  if (process_info) {
    b_deallocate(process_info);
  }
  return false;
}

B_WUR B_EXPORT_FUNC bool
b_process_controller_exec_basic(
    B_BORROW struct B_ProcessController *controller,
    B_BORROW char const *const *args,
    B_TRANSFER B_ProcessExitCallback *callback,
    B_TRANSFER void *callback_opaque,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(controller);
  B_PRECONDITION(args);
  B_PRECONDITION(args[0]);
  B_PRECONDITION(callback);
  B_OUT_PARAMETER(e);

#if B_CONFIG_POSIX_SPAWN
  bool attributes_initialized = false;
  int rc;

  posix_spawnattr_t attributes;
  rc = posix_spawnattr_init(&attributes);
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = rc};
    goto fail;
  }
  attributes_initialized = true;
  rc = posix_spawnattr_setflags(
    &attributes,
    POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = rc};
    goto fail;
  }
  {
    sigset_t signals;
    b_sigfillset(&signals);
#if B_CONFIG_PEDANTIC_POSIX_SPAWN_SIGNALS
    b_sigdelset(&signals, SIGKILL);
    b_sigdelset(&signals, SIGSTOP);
# if defined(SIGRTMIN) && defined(SIGRTMAX)
#  if defined(__SIGRTMIN)
    int start = __SIGRTMIN;
#  else
    int start = SIGRTMIN;
#  endif
    for (int i = start; i <= SIGRTMAX; ++i) {
      b_sigdelset(&signals, i);
    }
# endif
#endif
    rc = posix_spawnattr_setsigdefault(
      &attributes, &signals);
    if (rc != 0) {
      *e = (struct B_Error) {.posix_error = rc};
      goto fail;
    }
  }

  pid_t pid;
  posix_spawn_file_actions_t const *file_actions = NULL;
  char *const *envp = NULL;
  rc = posix_spawnp(
    &pid,
    args[0],
    file_actions,
    &attributes,
    // FIXME(strager): Cast looks like a bug!
    (char *const *) args,
    envp);
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = rc};
    goto fail;
  }

  if (!b_process_controller_register_process_id(
      controller, pid, callback, callback_opaque, e)) {
    // FIXME(strager): What do we do about the child
    // process?  Do we kill it?
    goto fail;
  }

  return true;

fail:
  if (attributes_initialized) {
    rc = posix_spawnattr_destroy(&attributes);
    B_ASSERT(rc == 0);
  }
  return false;
#else
# error "Unknown exec_basic implementation"
#endif
}
