#include <B/Error.h>
#include <B/Memory.h>
#include <B/Private/Assertions.h>
#include <B/Private/Callback.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/Private/Queue.h>
#include <B/RunLoop.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct B_RunLoopPlainFunctionEntry_ {
  B_SLIST_ENTRY(B_RunLoopPlainFunctionEntry_) link;
  B_RunLoopFunction *callback;
  B_RunLoopFunction *cancel_callback;
  union B_UserData user_data;
};

struct B_RunLoopPlain_ {
  struct B_RunLoop super;
  B_SLIST_HEAD(, B_RunLoopPlainFunctionEntry_) callbacks;
  bool stop;
};

B_STATIC_ASSERT(
  offsetof(struct B_RunLoopPlain_, super) == 0,
  "super must be the first member of B_RunLoopPlain_");

static B_WUR B_FUNC void
b_run_loop_deallocate_plain_(
    B_TRANSFER struct B_RunLoop *run_loop) {
  B_PRECONDITION(run_loop);

  struct B_RunLoopPlain_ *rl
    = (struct B_RunLoopPlain_ *) run_loop;
  struct B_RunLoopPlainFunctionEntry_ *callback_entry;
  struct B_RunLoopPlainFunctionEntry_ *temp_entry;
  B_SLIST_FOREACH_SAFE(
      callback_entry, &rl->callbacks, link, temp_entry) {
    if (!callback_entry->cancel_callback(
          &rl->super,
          callback_entry->user_data.bytes,
          &(struct B_Error) {})) {
      B_NYI();
    }
    b_deallocate(callback_entry);
  }
  b_deallocate(rl);
}

static B_WUR B_FUNC bool
b_run_loop_add_function_plain_(
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

  struct B_RunLoopPlain_ *rl
    = (struct B_RunLoopPlain_ *) run_loop;
  // TODO(strager): Check for overflow.
  size_t entry_size = offsetof(
    struct B_RunLoopPlainFunctionEntry_,
    user_data.bytes[callback_data_size]);
  struct B_RunLoopPlainFunctionEntry_ *entry;
  if (!b_allocate(entry_size, (void **) &entry, e)) {
    return false;
  }
  *entry = (struct B_RunLoopPlainFunctionEntry_) {
    // .link
    .callback = callback,
    .cancel_callback = cancel_callback,
    // .user_data
  };
  memcpy(
    entry->user_data.bytes,
    callback_data,
    callback_data_size);
  B_SLIST_INSERT_HEAD(&rl->callbacks, entry, link);
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_run_plain_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopPlain_ *rl
    = (struct B_RunLoopPlain_ *) run_loop;
  while (!rl->stop) {
    struct B_RunLoopPlainFunctionEntry_ *entry
      = B_SLIST_FIRST(&rl->callbacks);
    if (!entry) {
      fprintf(
        stderr,
        "No functions to run, but b_run_loop_stop not "
        "called\n");
      *e = (struct B_Error) {.posix_error = ENOTSUP};
      return false;
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
  return true;
}

static B_WUR B_FUNC bool
b_run_loop_stop_(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopPlain_ *rl
    = (struct B_RunLoopPlain_ *) run_loop;
  rl->stop = true;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_plain(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_RunLoopPlain_ *rl;
  if (!b_allocate(sizeof(*rl), (void **) &rl, e)) {
    return false;
  }
  *rl = (struct B_RunLoopPlain_) {
    .super = {
      .vtable = {
        .deallocate = b_run_loop_deallocate_plain_,
        .add_function = b_run_loop_add_function_plain_,
        .run = b_run_loop_run_plain_,
        .stop = b_run_loop_stop_,
      },
    },
    .stop = false,
    .callbacks = B_SLIST_HEAD_INITIALIZER(&rl->callbacks),
  };
  *out = &rl->super;
  return true;
}
