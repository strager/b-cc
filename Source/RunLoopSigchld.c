#include <B/Private/Config.h>

#if B_CONFIG_POSIX_SIGNALS && B_CONFIG_POSIX_PROCESS
# include <B/Error.h>
# include <B/Memory.h>
# include <B/Private/Assertions.h>
# include <B/Private/Callback.h>
# include <B/Private/Log.h>
# include <B/Private/Memory.h>
# include <B/Private/Queue.h>
# include <B/Private/RunLoopUtil.h>
# include <B/Process.h>
# include <B/RunLoop.h>

# if B_CONFIG_EVENTFD
#  define B_USE_EVENTFD_ 1
#  define B_USE_PIPE_ 0
# else
#  define B_USE_EVENTFD_ 0
#  define B_USE_PIPE_ 1
# endif

# include <errno.h>
# include <limits.h>
# include <poll.h>
# include <signal.h>
# include <string.h>
# include <sys/wait.h>
# include <unistd.h>

# if B_USE_EVENTFD_
#  include <sys/eventfd.h>
# endif

# if B_USE_PIPE_
#  include <fcntl.h>
# endif

struct B_RunLoopSigchldProcessEntry_ {
  B_SLIST_ENTRY(B_RunLoopSigchldProcessEntry_) link;
  union {
    // Set when this entry is in the RunLoop's processes
    // list.
    pid_t pid;

    // Set when this entry is removed from the RunLoop's
    // processes list.
    int waitpid_status;
  } u;
  B_RunLoopProcessFunction *callback;
  B_RunLoopFunction *cancel_callback;
  union B_UserData user_data;
};

struct B_RunLoopSigchld_ {
  struct B_RunLoop super;
# if B_USE_EVENTFD_
  int functions_eventfd;
# endif
# if B_USE_PIPE_
  int functions_pipe_read;
  int functions_pipe_write;
# endif
  bool stop;
  B_RunLoopFunctionList functions;
  B_SLIST_HEAD(, B_RunLoopSigchldProcessEntry_) processes;
};

B_STATIC_ASSERT(
  offsetof(struct B_RunLoopSigchld_, super) == 0,
  "super must be the first member of B_RunLoopSigchld_");

# if B_USE_EVENTFD_
static int
b_sigchld_eventfd_ = -1;
# endif

# if B_USE_PIPE_
static int
b_sigchld_pipe_read_ = -1;
static int
b_sigchld_pipe_write_ = -1;
# endif

B_EXPORT_FUNC_CDECL void
b_run_loop_sigchld_handler(
    int signal_number) {
  B_PRECONDITION(signal_number == SIGCHLD);

# if B_USE_EVENTFD_
  B_ASSERT(b_sigchld_eventfd_ != -1);
  int rc = eventfd_write(b_sigchld_eventfd_, 1);
  B_ASSERT(rc != -1);
# endif
# if B_USE_PIPE_
  B_ASSERT(b_sigchld_pipe_write_ != -1);
  uint8_t data[1] = {0};
  ssize_t rc = write(
    b_sigchld_pipe_write_, data, sizeof(data));
  if (rc == -1) {
    if (errno == EAGAIN) {
      // Pipe is full. Run loop will be woken up anyway.
      // Ignore.
    } else {
      B_BUG();
    }
  }
# endif
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_sigchld_handler_initialize(
    struct B_Error *e) {
  B_OUT_PARAMETER(e);

  // TODO(strager): Make thread-safe.
# if B_USE_EVENTFD_
  if (b_sigchld_eventfd_ != -1) {
    // Already initialized.
    return true;
  }
  int fd = eventfd(0, EFD_CLOEXEC);
  if (fd == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  }
  b_sigchld_eventfd_ = fd;
  return true;
# endif
# if B_USE_PIPE_
  if (b_sigchld_pipe_read_ != -1) {
    // Already initialized.
    return true;
  }
  int fds[2];
  int rc = pipe(fds);
  if (rc == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  }
  b_sigchld_pipe_read_ = fds[0];
  b_sigchld_pipe_write_ = fds[1];
  return true;
# endif
}

static B_WUR B_FUNC void
b_run_loop_deallocate_(
    B_TRANSFER struct B_RunLoop *run_loop) {
  B_PRECONDITION(run_loop);

  struct B_RunLoopSigchld_ *rl
    = (struct B_RunLoopSigchld_ *) run_loop;
  b_run_loop_function_list_deinitialize(
    run_loop, &rl->functions);
# if B_USE_EVENTFD_
  (void) close(rl->functions_eventfd);
# endif
# if B_USE_PIPE_
  (void) close(rl->functions_pipe_read);
  (void) close(rl->functions_pipe_write);
# endif
  b_deallocate(rl);
}

static B_WUR B_FUNC bool
b_run_loop_notify_(
    B_BORROW struct B_RunLoopSigchld_ *rl,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(rl);
  B_OUT_PARAMETER(e);

# if B_USE_EVENTFD_
  int rc = eventfd_write(rl->functions_eventfd, 1);
  if (rc == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  }
  return true;
# endif
# if B_USE_PIPE_
  uint8_t data[1] = {0};
  ssize_t rc = write(
    rl->functions_pipe_write, data, sizeof(data));
  if (rc == -1) {
    if (errno == EAGAIN) {
      // Pipe is full. Run loop will be woken up anyway.
      // Ignore.
    } else {
      *e = (struct B_Error) {.posix_error = errno};
      return false;
    }
  }
  return true;
# endif
}

static B_WUR B_FUNC bool
b_run_loop_add_function_(
    B_BORROW struct B_RunLoop *run_loop,
    B_TRANSFER B_RunLoopFunction *callback,
    B_TRANSFER B_RunLoopFunction *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel_callback);
  B_OUT_PARAMETER(e);

  struct B_RunLoopSigchld_ *rl
    = (struct B_RunLoopSigchld_ *) run_loop;
  if (!b_run_loop_function_list_add_function(
      &rl->functions,
      callback,
      cancel_callback,
      callback_data,
      callback_data_size,
      e)) {
    return false;
  }
  if (!b_run_loop_notify_(rl, e)) {
    // Leave the function enqueued. Another thread could
    // have popped it, and the worst that can happen for a
    // failed notify is a deadlock.
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_add_process_id_(
    B_BORROW struct B_RunLoop *run_loop,
    B_ProcessID pid,
    B_TRANSFER B_RunLoopProcessFunction *callback,
    B_TRANSFER B_RunLoopFunction *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel_callback);
  B_OUT_PARAMETER(e);

  struct B_RunLoopSigchld_ *rl
    = (struct B_RunLoopSigchld_ *) run_loop;
  if (pid >= (1L << ((sizeof(pid_t) * CHAR_BIT) - 1))) {
    *e = (struct B_Error) {.posix_error = EINVAL};
    return false;
  }
  pid_t native_pid = (pid_t) pid;
  // TODO(strager): Check for overflow.
  size_t entry_size = offsetof(
    struct B_RunLoopSigchldProcessEntry_,
    user_data.bytes[callback_data_size]);
  struct B_RunLoopSigchldProcessEntry_ *entry;
  if (!b_allocate(entry_size, (void **) &entry, e)) {
    return false;
  }
  entry->u.pid = native_pid;
  entry->callback = callback;
  entry->cancel_callback = cancel_callback;
  if (callback_data) {
    memcpy(
      entry->user_data.bytes,
      callback_data,
      callback_data_size);
  }
  B_SLIST_INSERT_HEAD(&rl->processes, entry, link);
  return true;
}

static B_FUNC void
b_run_loop_drain_functions_(
    B_BORROW struct B_RunLoopSigchld_ *rl) {
  B_PRECONDITION(rl);

  bool keep_going = true;
  while (keep_going && !rl->stop) {
    b_run_loop_function_list_run_one(
      &rl->super, &rl->functions, &keep_going);
  }
}

static B_FUNC bool
b_run_loop_check_process_(
    pid_t pid,
    B_OUT bool *exited,
    B_OUT int *waitpid_status,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(exited);
  B_OUT_PARAMETER(waitpid_status);
  B_OUT_PARAMETER(e);

  int status;
  pid_t rc = waitpid(pid, &status, WNOHANG);
  if (rc == -1) {
    // FIXME(strager): Can we get EINTR here?
    B_ASSERT(errno != EINTR);
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  } else if (rc == 0) {
    *exited = false;
    return true;
  } else {
    B_ASSERT(rc == pid);
    *exited = true;
    *waitpid_status = status;
    return true;
  }
}

static B_FUNC void
b_run_loop_check_processes_(
    B_BORROW struct B_RunLoopSigchld_ *rl) {
  B_PRECONDITION(rl);

  B_SLIST_HEAD(, B_RunLoopSigchldProcessEntry_)
    exited_processes;
  B_SLIST_INIT(&exited_processes);
  struct B_RunLoopSigchldProcessEntry_ *entry;
  struct B_RunLoopSigchldProcessEntry_ *temp_entry;

  // Put all exited processes in exited_processes.
  struct B_RunLoopSigchldProcessEntry_ *prev = NULL;
  B_SLIST_FOREACH_SAFE(
      entry, &rl->processes, link, temp_entry) {
    int waitpid_status;
    bool exited;
    if (!b_run_loop_check_process_(
        entry->u.pid,
        &exited,
        &waitpid_status,
        &(struct B_Error) {.posix_error = 0})) {
      B_NYI();
    }
    if (exited) {
      if (prev) {
        B_SLIST_REMOVE_AFTER(prev, link);
      } else {
        B_SLIST_REMOVE_HEAD(&rl->processes, link);
      }
      B_SLIST_INSERT_HEAD(&exited_processes, entry, link);
      entry->u.waitpid_status = waitpid_status;
    } else {
      prev = entry;
    }
  }

  B_SLIST_FOREACH_SAFE(
      entry, &exited_processes, link, temp_entry) {
    struct B_ProcessExitStatus exit_status
      = b_exit_status_from_waitpid_status(
        entry->u.waitpid_status);
    if (!entry->callback(
        &rl->super,
        &exit_status,
        entry->user_data.bytes,
        &(struct B_Error) {.posix_error = 0})) {
      B_NYI();
    }
    b_deallocate(entry);
  }
}

static B_WUR B_FUNC bool
b_run_loop_run_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  // TODO(strager): Produce good diagnostics.
# if B_USE_EVENTFD_
  B_ASSERT(b_sigchld_eventfd_ != -1);
# endif
# if B_USE_PIPE_
  B_ASSERT(b_sigchld_pipe_read_ != -1);
  B_ASSERT(b_sigchld_pipe_write_ != -1);
# endif

  struct B_RunLoopSigchld_ *rl
    = (struct B_RunLoopSigchld_ *) run_loop;
# if B_USE_EVENTFD_
  int sigchld_fd = b_sigchld_eventfd_;
  int functions_fd = rl->functions_eventfd;
# endif
# if B_USE_PIPE_
  int sigchld_fd = b_sigchld_pipe_read_;
  int functions_fd = rl->functions_pipe_read;
# endif
  while (!rl->stop) {
    struct pollfd pollfds[] = {
      {.fd = sigchld_fd, .events = POLLIN},
      {.fd = functions_fd, .events = POLLIN},
    };
    nfds_t pollfd_count
      = sizeof(pollfds) / sizeof(*pollfds);
    int events = poll(pollfds, pollfd_count, -1);
    bool check_functions = false;
    bool check_processes = false;
    if (events == -1) {
      if (errno == EINTR) {
        // Possibly interrupted by SIGCHLD.
        check_processes = true;
      } else {
        *e = (struct B_Error) {.posix_error = errno};
        return false;
      }
    } else {
      for (nfds_t i = 0; i < pollfd_count; ++i) {
        if (pollfds[i].revents & POLLIN) {
          if (pollfds[i].fd == sigchld_fd) {
            check_processes = true;
          } else if (pollfds[i].fd == functions_fd) {
            check_functions = true;
          } else {
            B_BUG();
          }
        }
      }
    }
    if (check_functions) {
      b_run_loop_drain_functions_(rl);
    }
    if (check_processes) {
      b_run_loop_check_processes_(rl);
    }
  }
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_stop_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopSigchld_ *rl
    = (struct B_RunLoopSigchld_ *) run_loop;
  rl->stop = true;
  if (!b_run_loop_notify_(rl, e)) {
    return false;
  }
  return true;
}

// HACK(strager)
# if defined(NSIG)
#  define B_NSIG_ NSIG
# elif defined(_NSIG)
#  define B_NSIG_ _NSIG
# elif defined(__DARWIN_NSIG)
#  define B_NSIG_ __DARWIN_NSIG
# else
#  error "Unknown NSIG implementation"
# endif

static B_WUR B_FUNC bool
b_sigaction_equal_(
    struct sigaction const *restrict a,
    struct sigaction const *restrict b) {
  B_PRECONDITION(a);
  B_PRECONDITION(b);

  if (a->sa_flags != b->sa_flags) {
    return false;
  }
  if (a->sa_flags & SA_SIGINFO) {
    if (a->sa_sigaction != b->sa_sigaction) {
      return false;
    }
  } else {
    if (a->sa_handler != b->sa_handler) {
      return false;
    }
  }
  for (int i = 1; i < B_NSIG_; ++i) {
    if (sigismember(&a->sa_mask, i)
        != sigismember(&b->sa_mask, i)) {
      return false;
    }
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_sigchld(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  // FIXME(strager): Figure out a way to test this
  // atomically.
  int rc;
  struct sigaction old_action;
  rc = sigaction(SIGCHLD, NULL, &old_action);
  if (rc == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  }
  if (!(old_action.sa_flags & SA_SIGINFO)
      && old_action.sa_handler == SIG_DFL) {
    // Default signal handler. Okay to override.
    if (!b_run_loop_sigchld_handler_initialize(e)) {
      return false;
    }
    struct sigaction action;
    struct sigaction tmp_action;
    action.sa_flags
      = SA_NOCLDSTOP | SA_RESTART;
    action.sa_handler = b_run_loop_sigchld_handler;
    rc = sigemptyset(&action.sa_mask);
    B_ASSERT(rc == 0);
    rc = sigaction(SIGCHLD, &action, &tmp_action);
    if (rc == -1) {
      *e = (struct B_Error) {.posix_error = errno};
      return false;
    }
    // If this assertion fires, we hit a race condition.
    B_ASSERT(b_sigaction_equal_(&tmp_action, &old_action));

    if (!b_run_loop_allocate_sigchld_no_install(out, e)) {
      // Unset the signal handler.
      rc = sigaction(SIGCHLD, &old_action, &tmp_action);
      if (rc != -1) {
        // If this assertion fires, we hit a race condition.
        B_ASSERT(b_sigaction_equal_(&tmp_action, &action));
      }
      return false;
    }
    return true;
  } else if (!(old_action.sa_flags & SA_SIGINFO)
      && old_action.sa_handler
        == b_run_loop_sigchld_handler) {
    // Signal handler already installed.
    if (!b_run_loop_allocate_sigchld_no_install(out, e)) {
      return false;
    }
    return true;
  } else {
    // Non-default signal handler. Don't override.
    *e = (struct B_Error) {.posix_error = ENOTSUP};
    return false;
  }
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_sigchld_no_install(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

# if B_USE_EVENTFD_
  int functions_eventfd = -1;
# endif
# if B_USE_PIPE_
  int fds[2] = {-1, -1};
# endif
  struct B_RunLoopSigchld_ *rl = NULL;

# if B_USE_EVENTFD_
  functions_eventfd = eventfd(0, EFD_CLOEXEC);
  if (functions_eventfd == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    goto fail;
  }
# endif
# if B_USE_PIPE_
  int rc;
  rc = pipe(fds);
  if (rc == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    fds[0] = -1;
    fds[1] = -1;
    goto fail;
  }
  for (size_t i = 0; i < 2; ++i) {
    rc = fcntl(fds[i], F_SETFD, FD_CLOEXEC);
    if (rc == -1) {
      *e = (struct B_Error) {.posix_error = errno};
      goto fail;
    }
    rc = fcntl(fds[i], F_SETFL, O_NONBLOCK);
    if (rc == -1) {
      *e = (struct B_Error) {.posix_error = errno};
      goto fail;
    }
  }
# endif
  if (!b_allocate(sizeof(*rl), (void **) &rl, e)) {
    rl = NULL;
    goto fail;
  }
  *rl = (struct B_RunLoopSigchld_) {
    .super = {
      .vtable = {
        .deallocate = b_run_loop_deallocate_,
        .add_function = b_run_loop_add_function_,
        .add_process_id = b_run_loop_add_process_id_,
        .run = b_run_loop_run_,
        .stop = b_run_loop_stop_,
      },
    },
# if B_USE_EVENTFD_
    .functions_eventfd = functions_eventfd,
# endif
# if B_USE_PIPE_
    .functions_pipe_read = fds[0],
    .functions_pipe_write = fds[1],
# endif
    .stop = false,
    // .functions
    .processes = B_SLIST_HEAD_INITIALIZER(&rl->processes),
  };
  b_run_loop_function_list_initialize(&rl->functions);
  *out = &rl->super;
  return true;

fail:
# if B_USE_EVENTFD_
  if (functions_eventfd != -1) {
    (void) close(functions_eventfd);
  }
# endif
# if B_USE_PIPE_
  for (size_t i = 0; i < 2; ++i) {
    if (fds[i] != -1) {
      (void) close(fds[i]);
    }
  }
# endif
  if (rl) {
    b_deallocate(rl);
  }
  return false;
}

#else
# include <B/Error.h>
# include <B/Private/Assertions.h>
# include <B/RunLoop.h>

# include <errno.h>

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_sigchld(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *e = (struct B_Error) {.posix_error = ENOTSUP};
  return false;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_sigchld_no_install(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *e = (struct B_Error) {.posix_error = ENOTSUP};
  return false;
}
#endif
