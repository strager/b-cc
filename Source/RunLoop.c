#include <B/Error.h>
#include <B/Memory.h>
#include <B/Private/Assertions.h>
#include <B/Private/Callback.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/Private/Queue.h>
#include <B/Private/RunLoop.h>

#include <string.h>

struct B_RunLoopCallbackEntry_ {
  B_SLIST_ENTRY(B_RunLoopCallbackEntry_) link;
  B_RunLoopCallback *callback;
  B_RunLoopCallback *cancel_callback;
  union B_UserData user_data;
};

struct B_RunLoop {
  B_SLIST_HEAD(, B_RunLoopCallbackEntry_) callbacks;
};

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_RunLoop *run_loop;
  if (!b_allocate(
      sizeof(*run_loop), (void **) &run_loop, e)) {
    return false;
  }
  *run_loop = (struct B_RunLoop) {
    .callbacks = B_SLIST_HEAD_INITIALIZER(
      &run_loop->callbacks),
  };
  *out = run_loop;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_deallocate(
    B_TRANSFER struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  struct B_RunLoopCallbackEntry_ *callback_entry;
  struct B_RunLoopCallbackEntry_ *temp_entry;
  B_SLIST_FOREACH_SAFE(
      callback_entry,
      &run_loop->callbacks,
      link,
      temp_entry) {
    if (!callback_entry->cancel_callback(
          run_loop,
          callback_entry->user_data.bytes,
          &(struct B_Error) {})) {
      B_NYI();
    }
    b_deallocate(callback_entry);
  }
  b_deallocate(run_loop);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_add_callback(
    B_BORROW struct B_RunLoop *run_loop,
    B_TRANSFER B_RunLoopCallback *callback,
    B_TRANSFER B_RunLoopCallback *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel_callback);
  B_PRECONDITION(callback_data);
  B_OUT_PARAMETER(e);

  // TODO(strager): Check for overflow.
  size_t entry_size = offsetof(
      struct B_RunLoopCallbackEntry_,
      user_data.bytes[callback_data_size]);
  struct B_RunLoopCallbackEntry_ *entry;
  if (!b_allocate(entry_size, (void **) &entry, e)) {
    return false;
  }
  *entry = (struct B_RunLoopCallbackEntry_) {
    // .link
    .callback = callback,
    .cancel_callback = cancel_callback,
    // .user_data
  };
  memcpy(
    entry->user_data.bytes,
    callback_data,
    callback_data_size);
  B_SLIST_INSERT_HEAD(&run_loop->callbacks, entry, link);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_step(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT bool *keep_going,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(keep_going);
  B_OUT_PARAMETER(e);

  struct B_RunLoopCallbackEntry_ *entry
    = B_SLIST_FIRST(&run_loop->callbacks);
  if (entry) {
    B_SLIST_REMOVE_HEAD(&run_loop->callbacks, link);
    if (!entry->callback(
        run_loop,
        entry->user_data.bytes,
        &(struct B_Error) {})) {
      B_NYI();
    }
    b_deallocate(entry);
    *keep_going = true;
    return true;
  } else {
    *keep_going = false;
    return true;
  }
}
