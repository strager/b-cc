#include <B/Error.h>
#include <B/Memory.h>
#include <B/Private/Assertions.h>
#include <B/Private/Callback.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/Private/RunLoopUtil.h>

#include <stddef.h>
#include <string.h>

#if B_CONFIG_POSIX_PROCESS
# include <sys/wait.h>
#endif

struct B_RunLoopFunctionEntry {
  B_SLIST_ENTRY(B_RunLoopFunctionEntry) link;
  B_RunLoopFunction *callback;
  B_RunLoopFunction *cancel_callback;
  union B_UserData user_data;
};

B_FUNC void
b_run_loop_function_list_initialize(
    B_BORROW B_RunLoopFunctionList *functions) {
  B_PRECONDITION(functions);

  B_SLIST_INIT(functions);
}

B_FUNC void
b_run_loop_function_list_deinitialize(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW B_RunLoopFunctionList *functions) {
  B_PRECONDITION(rl);
  B_PRECONDITION(functions);

  struct B_RunLoopFunctionEntry *entry;
  struct B_RunLoopFunctionEntry *temp_entry;
  B_SLIST_FOREACH_SAFE(
      entry, functions, link, temp_entry) {
    if (!entry->cancel_callback(
        rl,
        entry->user_data.bytes,
        &(struct B_Error) {.posix_error = 0})) {
      B_NYI();
    }
    b_deallocate(entry);
    // FIXME(strager): Should we keep iterating until the
    // list is empty? What if the list is mutated while we
    // iterate?
  }
  b_scribble(functions, sizeof(*functions));
}

B_WUR B_FUNC bool
b_run_loop_function_list_add_function(
    B_BORROW B_RunLoopFunctionList *functions,
    B_TRANSFER B_RunLoopFunction *callback,
    B_TRANSFER B_RunLoopFunction *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(functions);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel_callback);
  B_OUT_PARAMETER(e);

  // TODO(strager): Check for overflow.
  size_t entry_size = offsetof(
    struct B_RunLoopFunctionEntry,
    user_data.bytes[callback_data_size]);
  struct B_RunLoopFunctionEntry *entry;
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
  B_SLIST_INSERT_HEAD(functions, entry, link);
  return true;
}

B_FUNC void
b_run_loop_function_list_run_one(
    B_BORROW struct B_RunLoop *run_loop,
    B_BORROW B_RunLoopFunctionList *functions,
    B_OUT bool *keep_going) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(functions);
  B_OUT_PARAMETER(keep_going);

  struct B_RunLoopFunctionEntry *entry
    = B_SLIST_FIRST(functions);
  if (!entry) {
    *keep_going = false;
    return;
  }
  B_SLIST_REMOVE_HEAD(functions, link);
  if (!entry->callback(
      run_loop,
      entry->user_data.bytes,
      &(struct B_Error) {.posix_error = 0})) {
    B_NYI();
  }
  b_deallocate(entry);
  *keep_going = true;
}

#if B_CONFIG_POSIX_PROCESS
B_WUR B_FUNC struct B_ProcessExitStatus
b_exit_status_from_waitpid_status(
    int waitpid_status) {
  // N.B. We can't use initializers here because older
  // compilers (like GCC 4.4) don't support initializers
  // with anonymous struct fields.
  if (WIFEXITED(waitpid_status)) {
    struct B_ProcessExitStatus exit_status;
    exit_status.type = B_PROCESS_EXIT_STATUS_CODE;
    exit_status.u.code.exit_code
      = WEXITSTATUS(waitpid_status);
    return exit_status;
  } else if (WIFSIGNALED(waitpid_status)) {
    struct B_ProcessExitStatus exit_status;
    exit_status.type = B_PROCESS_EXIT_STATUS_SIGNAL;
    exit_status.u.signal.signal_number
      = WTERMSIG(waitpid_status);
    return exit_status;
  } else {
    B_ASSERT(!WIFSTOPPED(waitpid_status));
    B_BUG();
  }
}
#endif
