#include <B/Private/Config.h>

#if B_CONFIG_KQUEUE
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

# include <errno.h>
# include <stddef.h>
# include <string.h>
# include <sys/event.h>
# include <sys/wait.h>
# include <unistd.h>

enum {
  // ident for EVFILT_USER (auto-reset event). triggered
  // when a callback is added or when stop is changed.
  B_RUN_LOOP_USER_IDENT_ = 1,
};

struct B_RunLoopKqueueProcessEntry_ {
  B_RunLoopProcessFunction *callback;
  B_RunLoopFunction *cancel_callback;
  union B_UserData user_data;
};

struct B_RunLoopKqueue_ {
  struct B_RunLoop super;
  int fd;
  bool stop;
  B_RunLoopFunctionList functions;
};

B_STATIC_ASSERT(
  offsetof(struct B_RunLoopKqueue_, super) == 0,
  "super must be the first member of B_RunLoopKqueue_");

static B_WUR B_FUNC void
b_run_loop_deallocate_(
    B_TRANSFER struct B_RunLoop *run_loop) {
  B_PRECONDITION(run_loop);

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
  b_run_loop_function_list_deinitialize(
    run_loop, &rl->functions);
  (void) close(rl->fd);
  b_deallocate(rl);
}

static B_WUR B_FUNC bool
b_run_loop_notify_(
    B_BORROW struct B_RunLoopKqueue_ *rl,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(rl);
  B_OUT_PARAMETER(e);

  int rc = kevent(rl->fd, &(struct kevent) {
    .ident = B_RUN_LOOP_USER_IDENT_,
    .filter = EVFILT_USER,
    .flags = 0,
    .fflags = NOTE_TRIGGER,
    .data = (intptr_t) NULL,
    .udata = NULL,
  }, 1, NULL, 0, NULL);
  if (rc == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  }
  B_ASSERT(rc == 0);
  return true;
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

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
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

struct B_RunLoopKqueueProcessFallbackClosure_ {
  struct B_ProcessExitStatus exit_status;
  B_RunLoopProcessFunction *callback;
  B_RunLoopFunction *cancel_callback;
  union B_UserData user_data;
};

static B_FUNC bool
b_run_loop_add_process_id_kqueue_fallback_callback_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_RunLoopKqueueProcessFallbackClosure_ *closure
    = *(struct B_RunLoopKqueueProcessFallbackClosure_ *
        const *) callback_data;
  bool ok = closure->callback(
    rl, &closure->exit_status, closure->user_data.bytes, e);
  b_deallocate(closure);
  return ok;
}

static B_FUNC bool
b_run_loop_add_process_id_kqueue_fallback_cancel_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_RunLoopKqueueProcessFallbackClosure_ *closure
    = *(struct B_RunLoopKqueueProcessFallbackClosure_ *
        const *) callback_data;
  bool ok = closure->cancel_callback(
    rl, closure->user_data.bytes, e);
  b_deallocate(closure);
  return ok;
}

static B_WUR B_FUNC bool
b_run_loop_add_process_id_kqueue_fallback_(
    B_BORROW struct B_RunLoopKqueue_ *rl,
    B_BORROW struct B_ProcessExitStatus const *exit_status,
    B_TRANSFER B_RunLoopProcessFunction *callback,
    B_TRANSFER B_RunLoopFunction *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(rl);
  B_PRECONDITION(exit_status);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel_callback);
  B_OUT_PARAMETER(e);

  size_t closure_size = offsetof(
    struct B_RunLoopKqueueProcessFallbackClosure_,
    user_data.bytes[callback_data_size]);
  struct B_RunLoopKqueueProcessFallbackClosure_ *closure;
  if (!b_allocate(closure_size, (void **) &closure, e)) {
    return false;
  }
  closure->exit_status = *exit_status;
  closure->callback = callback;
  closure->cancel_callback = cancel_callback;
  if (callback_data) {
    memcpy(
      closure->user_data.bytes,
      callback_data,
      callback_data_size);
  }
  if (!b_run_loop_add_function_(
      (struct B_RunLoop *) rl,
      b_run_loop_add_process_id_kqueue_fallback_callback_,
      b_run_loop_add_process_id_kqueue_fallback_cancel_,
      &closure,
      sizeof(closure_size),
      e)) {
    b_deallocate(closure);
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

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
  // TODO(strager): Check for overflow.
  size_t entry_size = offsetof(
    struct B_RunLoopKqueueProcessEntry_,
    user_data.bytes[callback_data_size]);
  struct B_RunLoopKqueueProcessEntry_ *entry;
  if (!b_allocate(entry_size, (void **) &entry, e)) {
    return false;
  }
  entry->callback = callback;
  entry->cancel_callback = cancel_callback;
  if (callback_data) {
    memcpy(
      entry->user_data.bytes,
      callback_data,
      callback_data_size);
  }
  int rc = kevent(rl->fd, &(struct kevent) {
    .ident = pid,
    .filter = EVFILT_PROC,
    .flags = EV_ADD | EV_ONESHOT,
    .fflags = NOTE_EXIT | NOTE_EXITSTATUS,
    .data = (intptr_t) NULL,
    .udata = entry,
  }, 1, NULL, 0, NULL);
  if (rc != -1) {
    B_ASSERT(rc == 0);
    return true;
  }
  b_deallocate(entry);
  if (errno == ESRCH) {
    // If the process exits before it's added to a kqueue,
    // kevent will fail with ESRCH.
    int status;
retry:;
    pid_t new_pid = waitpid(pid, &status, WNOHANG);
    if (new_pid == -1) {
      if (errno == EINTR) {
        goto retry;
      }
      *e = (struct B_Error) {.posix_error = errno};
      return false;
    }
    B_ASSERT(new_pid == pid);
    struct B_ProcessExitStatus exit_status
      = b_exit_status_from_waitpid_status(status);
    if (!b_run_loop_add_process_id_kqueue_fallback_(
        rl,
        &exit_status,
        callback,
        cancel_callback,
        callback_data,
        callback_data_size,
        e)) {
      return false;
    }
    return true;
  } else {
    *e = (struct B_Error) {.posix_error = errno};
    return false;
  }
}

static B_FUNC void
b_run_loop_drain_functions_(
    B_BORROW struct B_RunLoopKqueue_ *rl) {
  B_PRECONDITION(rl);

  bool keep_going = true;
  while (keep_going && !rl->stop) {
    b_run_loop_function_list_run_one(
      &rl->super, &rl->functions, &keep_going);
  }
}

static B_WUR B_FUNC bool
b_run_loop_run_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
  while (!rl->stop) {
    struct kevent events[10];
    int event_count = kevent(
      rl->fd,
      NULL,
      0,
      events,
      sizeof(events) / sizeof(*events),
      NULL);
    if (event_count == -1) {
      *e = (struct B_Error) {.posix_error = errno};
      return false;
    }
    B_ASSERT(
      (size_t) event_count
        <= sizeof(events) / sizeof(*events));
    for (int i = 0; i < event_count; ++i) {
      switch (events[i].filter) {
      case EVFILT_USER:
        B_ASSERT(events[i].ident
          == B_RUN_LOOP_USER_IDENT_);
        break;
      case EVFILT_PROC:
        if (events[i].fflags & NOTE_EXIT) {
          struct B_RunLoopKqueueProcessEntry_ *entry
            = (struct B_RunLoopKqueueProcessEntry_ *)
              events[i].udata;
          struct B_ProcessExitStatus s
            = b_exit_status_from_waitpid_status(
              events[i].data);
          if (!entry->callback(
              &rl->super,
              &s,
              entry->user_data.bytes,
              &(struct B_Error) {.posix_error = 0})) {
            B_NYI();
          }
          b_deallocate(entry);
        }
        break;
      default:
        B_BUG();
      }
    }

    b_run_loop_drain_functions_(rl);
  }
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_stop_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
  rl->stop = true;
  if (!b_run_loop_notify_(rl, e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_kqueue(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  int fd = -1;
  struct B_RunLoopKqueue_ *rl = NULL;

  fd = kqueue();
  if (fd == -1) {
    *e = (struct B_Error) {.posix_error = errno};
    goto fail;
  }
  int rc = kevent(fd, &(struct kevent) {
    .ident = B_RUN_LOOP_USER_IDENT_,
    .filter = EVFILT_USER,
    .flags = EV_ADD | EV_CLEAR,
    .fflags = NOTE_FFNOP,
    .data = (intptr_t) NULL,
    .udata = NULL,
  }, 1, NULL, 0, NULL);
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = errno};
    goto fail;
  }
  if (!b_allocate(sizeof(*rl), (void **) &rl, e)) {
    rl = NULL;
    goto fail;
  }
  *rl = (struct B_RunLoopKqueue_) {
    .super = {
      .vtable = {
        .deallocate = b_run_loop_deallocate_,
        .add_function = b_run_loop_add_function_,
        .add_process_id = b_run_loop_add_process_id_,
        .run = b_run_loop_run_,
        .stop = b_run_loop_stop_,
      },
    },
    .fd = fd,
    .stop = false,
    // .functions
  };
  b_run_loop_function_list_initialize(&rl->functions);
  *out = &rl->super;
  return true;

fail:
  if (fd != -1) {
    (void) close(fd);
  }
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
b_run_loop_allocate_kqueue(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *e = (struct B_Error) {.posix_error = ENOTSUP};
  return false;
}
#endif
