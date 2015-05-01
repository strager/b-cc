#include <B/Private/Config.h>

#if B_CONFIG_KQUEUE
# include <B/Error.h>
# include <B/Memory.h>
# include <B/Private/Assertions.h>
# include <B/Private/Callback.h>
# include <B/Private/Log.h>
# include <B/Private/Memory.h>
# include <B/Private/Queue.h>
# include <B/RunLoop.h>

# include <errno.h>
# include <stddef.h>
# include <string.h>
# include <sys/event.h>
# include <unistd.h>

enum {
  // ident for EVFILT_USER (auto-reset event). triggered
  // when a callback is added or when stop is changed.
  B_RUN_LOOP_USER_IDENT_ = 1,
};

struct B_RunLoopKqueueFunctionEntry_ {
  B_SLIST_ENTRY(B_RunLoopKqueueFunctionEntry_) link;
  B_RunLoopFunction *callback;
  B_RunLoopFunction *cancel_callback;
  union B_UserData user_data;
};

struct B_RunLoopKqueue_ {
  struct B_RunLoop super;
  int fd;
  bool stop;
  B_SLIST_HEAD(, B_RunLoopKqueueFunctionEntry_) callbacks;
};

B_STATIC_ASSERT(
  offsetof(struct B_RunLoopKqueue_, super) == 0,
  "super must be the first member of B_RunLoopKqueue_");

static B_WUR B_FUNC void
b_run_loop_deallocate_kqueue_(
    B_TRANSFER struct B_RunLoop *run_loop) {
  B_PRECONDITION(run_loop);

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
  struct B_RunLoopKqueueFunctionEntry_ *callback_entry;
  struct B_RunLoopKqueueFunctionEntry_ *temp_entry;
  B_SLIST_FOREACH_SAFE(
      callback_entry, &rl->callbacks, link, temp_entry) {
    if (!callback_entry->cancel_callback(
          &rl->super,
          callback_entry->user_data.bytes,
          &(struct B_Error) {})) {
      B_NYI();
    }
    b_deallocate(callback_entry);
    // FIXME(strager): Should we keep iterating until the
    // list is empty? What if the list is mutated while we
    // iterate?
  }
  (void) close(rl->fd);
  b_deallocate(rl);
}

static B_WUR B_FUNC bool
b_run_loop_kqueue_notify_(
    B_BORROW struct B_RunLoopKqueue_ *rl,
    B_OUT struct B_Error *e) {
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
b_run_loop_add_function_kqueue_(
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
  // TODO(strager): Check for overflow.
  size_t entry_size = offsetof(
    struct B_RunLoopKqueueFunctionEntry_,
    user_data.bytes[callback_data_size]);
  struct B_RunLoopKqueueFunctionEntry_ *entry;
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
  B_SLIST_INSERT_HEAD(&rl->callbacks, entry, link);
  if (!b_run_loop_kqueue_notify_(rl, e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_run_kqueue_(
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
      event_count <= sizeof(events) / sizeof(*events));
    for (int i = 0; i < event_count; ++i) {
      switch (events[i].filter) {
      case EVFILT_USER:
        B_ASSERT(events[i].ident == B_RUN_LOOP_USER_IDENT_);
        break;
      default:
        B_BUG();
      }
    }

    // Drain all functions.
    while (!rl->stop) {
      struct B_RunLoopKqueueFunctionEntry_ *entry
        = B_SLIST_FIRST(&rl->callbacks);
      if (!entry) {
        break;
      }
      B_SLIST_REMOVE_HEAD(&rl->callbacks, link);
      if (!entry->callback(
          &rl->super,
          entry->user_data.bytes,
          &(struct B_Error) {})) {
        B_NYI();
      }
      b_deallocate(entry);
    }
  }
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_stop_kqueue_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopKqueue_ *rl
    = (struct B_RunLoopKqueue_ *) run_loop;
  rl->stop = true;
  if (!b_run_loop_kqueue_notify_(rl, e)) {
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
        .deallocate = b_run_loop_deallocate_kqueue_,
        .add_function = b_run_loop_add_function_kqueue_,
        .run = b_run_loop_run_kqueue_,
        .stop = b_run_loop_stop_kqueue_,
      },
    },
    .fd = fd,
    .stop = false,
    .callbacks = B_SLIST_HEAD_INITIALIZER(&rl->callbacks),
  };
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
