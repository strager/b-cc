#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/Log.h>
#include <B/Macro.h>
#include <B/Private/Queue.h>
#include <B/Private/Thread.h>
#include <B/Process.h>

#if defined(B_CONFIG_POSIX_SPAWN)
# include <B/Private/OS.h>
#endif

#include <stddef.h>

#if defined(B_CONFIG_POSIX_PROCESS)
# include <errno.h>
# include <sys/wait.h>
#endif

#if defined(B_CONFIG_POSIX_SPAWN)
# include <errno.h>
# include <signal.h>
# include <spawn.h>
#endif

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

#if !defined(B_CONFIG_PTHREAD)
# error "pthread support is assumed for this B_ProcessManager implementation"
#endif

struct ProcessInfo_ {
    B_ProcessExitCallback *B_CONST_STRUCT_MEMBER callback;
    void *B_CONST_STRUCT_MEMBER callback_opaque;

    B_ProcessID B_CONST_STRUCT_MEMBER pid;
    B_OPT void *B_CONST_STRUCT_MEMBER handle;

    // If this ProcessInfo is owned by a ProcessController,
    // link is locked by the owning ProcessController's
    // lock.  Otherwise, link is unlocked.
    B_LIST_ENTRY(ProcessInfo_) link;

    // Used only when this ProcessInfo is *not* owned by a
    // ProcessController.
    struct B_ProcessExitStatus exit_status;
};

typedef B_LIST_HEAD(, ProcessInfo_) ProcessInfoList_;

struct B_ProcessController {
    struct B_Mutex lock;

    // Locked by lock.
    ProcessInfoList_ processes;
};

struct B_ProcessManager {
    B_BORROWED struct B_ProcessControllerDelegate *delegate;
    struct B_ProcessController controller;
};

// Checks if the ProcessInfo's process has died.  If so:
//
// 1. Remove the ProcessInfo from the owning
//    ProcessController list.
// 2. Add the ProcessInfo to exited_processes.
// 3. Update ProcessInfo::exit_status.
//
// Assumes the lock of the owning ProcessController is held.
static B_FUNC
process_manager_check_locked_(
        struct ProcessInfo_ *,
        ProcessInfoList_ *exited_processes,
        struct B_ErrorHandler const *);

#if defined(B_CONFIG_POSIX_PROCESS)
static struct B_ProcessExitStatus
exit_status_from_waitpid_status_(
        int waitpid_status) {
    if (WIFEXITED(waitpid_status)) {
        return (struct B_ProcessExitStatus) {
            .type = B_PROCESS_EXIT_STATUS_CODE,
            .code = {
                .exit_code = WEXITSTATUS(waitpid_status),
            },
        };
    } else if (WIFSIGNALED(waitpid_status)) {
        return (struct B_ProcessExitStatus) {
            .type = B_PROCESS_EXIT_STATUS_SIGNAL,
            .signal = {
                .signal_number = WTERMSIG(waitpid_status),
            },
        };
    } else {
        B_ASSERT(!WIFSTOPPED(waitpid_status));
        B_BUG();
    }
}
#endif

static struct B_ProcessManager *
controller_manager_(
        struct B_ProcessController *controller) {
    B_ASSERT(controller);
    return (void *) (((char *) controller)
        - offsetof(struct B_ProcessManager, controller));
}

B_EXPORT_FUNC
b_process_manager_allocate(
        struct B_ProcessControllerDelegate *delegate,
        B_OUTPTR struct B_ProcessManager **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, delegate);
    B_CHECK_PRECONDITION(eh, delegate->register_process_id
        || delegate->register_process_handle);
    B_CHECK_PRECONDITION(eh, !!delegate->register_process_id
        == !!delegate->unregister_process_id);
    B_CHECK_PRECONDITION(
        eh, !!delegate->register_process_handle
            == !!delegate->unregister_process_handle);
    B_CHECK_PRECONDITION(eh, out);

    struct B_ProcessManager *manager = NULL;
    bool mutex_initialized = false;

    if (!b_allocate(
            sizeof(*manager), (void **) &manager, eh)) {
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
    if (!b_mutex_initialize(
            &manager->controller.lock, eh)) {
        goto fail;
    }
    mutex_initialized = true;

    *out = manager;
    return true;

fail:
    if (mutex_initialized) {
        B_ASSERT(manager);
        (void) b_mutex_destroy(
            &manager->controller.lock, eh);
    }
    if (manager) {
        (void) b_deallocate(manager, eh);
    }
    return false;
}

B_EXPORT_FUNC
b_process_manager_deallocate(
        B_TRANSFER struct B_ProcessManager *manager,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, manager);

    bool ok = true;
    struct ProcessInfo_ *process_info;
    struct ProcessInfo_ *process_info_tmp;
    B_LIST_FOREACH_SAFE(
            process_info,
            &manager->controller.processes,
            link,
            process_info_tmp) {
        // FIXME(strager): Do we call the callback?  (Yes.)
        // How?  (No idea.)
        B_LOG(
            B_WARN,
            "Process found in deallocating ProcessManager; "
                "not yet implemented");
        B_LIST_REMOVE(process_info, link);
        ok = b_deallocate(process_info, eh);
    }
    if (!b_deallocate(manager, eh)) {
        ok = false;
    }
    return ok;
}

static B_FUNC
process_manager_check_locked_(
        struct ProcessInfo_ *process_info,
        ProcessInfoList_ *exited_processes,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(exited_processes);

#if defined(B_CONFIG_POSIX_PROCESS)
    pid_t pid = process_info->pid;

    int status;
retry:;
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
        // FIXME(strager): Calling eh is bad while the
        // lock is being held, since eh may call a
        // method on ProcessManager or
        // ProcessController.
        switch (B_RAISE_ERRNO_ERROR(
                eh, errno, "waitpid")) {
        default:
        case B_ERROR_ABORT:
            return false;
        case B_ERROR_IGNORE:
            return true;
        case B_ERROR_RETRY:
            goto retry;
        }
    } else {
        B_BUG();
    }
#else
# error "Unknown process implementation"
#endif
}

B_EXPORT_FUNC
b_process_manager_check(
        struct B_ProcessManager *manager,
        struct B_ErrorHandler const *eh) {
    bool ok = true;
    struct ProcessInfo_ *process_info;
    struct ProcessInfo_ *process_info_tmp;

    ProcessInfoList_ exited_processes
        = B_LIST_HEAD_INITIALIZER(&exited_processes);

    b_mutex_lock(&manager->controller.lock);
    B_LIST_FOREACH_SAFE(
            process_info,
            &manager->controller.processes,
            link,
            process_info_tmp) {
        if (!process_manager_check_locked_(
                process_info, &exited_processes, eh)) {
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
            B_LOG(
                B_DEBUG,
                "Process %ld exited with code %ld",
                (long) process_info->pid,
                (long) exit_status->code.exit_code);
            break;
        case B_PROCESS_EXIT_STATUS_EXCEPTION:
            B_LOG(
                B_DEBUG,
                "Process %ld exited with exception %#08X",
                (long) process_info->pid,
                (unsigned) exit_status->exception.code);
            break;
        case B_PROCESS_EXIT_STATUS_SIGNAL:
            B_LOG(
                B_DEBUG,
                "Process %ld exited with signal %d",
                (long) process_info->pid,
                (int) exit_status->signal.signal_number);
            break;
        default:
            B_BUG();
        }

        if (!process_info->callback(
                &process_info->exit_status,
                process_info->callback_opaque,
                eh)) {
            ok = false;
            // Keep going.
        }
        (void) b_deallocate(process_info, eh);
    }

    return ok;
}

B_EXPORT_FUNC
b_process_manager_get_controller(
        struct B_ProcessManager *manager,
        struct B_ProcessController **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, manager);
    B_CHECK_PRECONDITION(eh, out);

    *out = &manager->controller;
    return true;
}

B_EXPORT_FUNC
b_process_controller_register_process_id(
        struct B_ProcessController *controller,
        B_ProcessID process_id,
        B_ProcessExitCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, controller);
    B_CHECK_PRECONDITION(eh, process_id > 0);
    B_CHECK_PRECONDITION(eh, callback);

    struct ProcessInfo_ *process_info = NULL;
    bool process_info_in_list = false;

    struct B_ProcessManager *manager
        = controller_manager_(controller);
retry:
    if (!manager->delegate->register_process_id) {
        switch (B_RAISE_ERRNO_ERROR(
                eh, ENOTSUP, "b_process_controller_"
                    "register_process_id")) {
        case B_ERROR_IGNORE:
        case B_ERROR_ABORT:
            goto fail;
        case B_ERROR_RETRY:
            goto retry;
        }
    }

    if (!b_allocate(
            sizeof(*process_info),
            (void **) &process_info,
            eh)) {
        goto fail;
    }
    *process_info = (struct ProcessInfo_) {
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
            manager->delegate, process_id, eh)) {
        goto fail;
    }

    B_LOG(
        B_DEBUG,
        "Added process %ld to process manager %p",
        (long) process_id,
        (void *) manager);

    return true;

fail:
    if (process_info_in_list) {
        B_ASSERT(process_info);
        b_mutex_lock(&controller->lock);
        B_LIST_REMOVE(process_info, link);
        b_mutex_unlock(&controller->lock);
    }
    if (process_info) {
        (void) b_deallocate(process_info, eh);
    }
    return false;
}

B_EXPORT_FUNC
b_process_controller_exec_basic(
        struct B_ProcessController *controller,
        char const *const *args,
        B_ProcessExitCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, controller);
    B_CHECK_PRECONDITION(eh, args);
    B_CHECK_PRECONDITION(eh, args[0]);
    B_CHECK_PRECONDITION(eh, callback);

#if defined(B_CONFIG_POSIX_SPAWN)
    bool attributes_initialized = false;
    int rc;

    posix_spawnattr_t attributes;
    rc = posix_spawnattr_init(&attributes);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
            eh, rc, "posix_spawnattr_init");
        goto fail;
    }
    attributes_initialized = true;
    rc = posix_spawnattr_setflags(
        &attributes,
        POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
            eh, rc, "posix_spawnattr_setflags");
        goto fail;
    }
    {
        sigset_t signals;
        b_sigfillset(&signals);
#if defined(B_CONFIG_PEDANTIC_POSIX_SPAWN_SIGNALS)
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
            (void) B_RAISE_ERRNO_ERROR(
                eh, rc, "posix_spawnattr_setsigdefault");
            goto fail;
        }
    }

retry:;
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
        switch (B_RAISE_ERRNO_ERROR(
                eh, rc, "posix_spawnp")) {
        case B_ERROR_IGNORE:
        case B_ERROR_ABORT:
            goto fail;
        case B_ERROR_RETRY:
            goto retry;
        }
    }

    if (!b_process_controller_register_process_id(
            controller,
            pid,
            callback,
            callback_opaque,
            eh)) {
        // FIXME(strager): What do we do about the child
        // process?  Do we kill it?
        goto fail;
    }

    return true;

fail:
    if (attributes_initialized) {
        rc = posix_spawnattr_destroy(&attributes);
        if (rc != 0) {
            (void) B_RAISE_ERRNO_ERROR(
                eh, rc, "posix_spawnattr_destroy");
        }
    }
    return false;
#else
# error "Unknown exec_basic implementation"
#endif
}
