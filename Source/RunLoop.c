#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/RunLoop.h>

#include <errno.h>

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_preferred(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_RunLoop *run_loop;
  if (b_run_loop_allocate_kqueue(&run_loop, e)) {
    *out = run_loop;
    return true;
  }
  if (e->posix_error != ENOTSUP) {
    return false;
  }
  if (b_run_loop_allocate_sigchld(&run_loop, e)) {
    *out = run_loop;
    return true;
  }
  if (e->posix_error != ENOTSUP) {
    return false;
  }
  // We must have *some* implementation. If not, that's a
  // pretty serious bug.
  B_BUG();
}

B_WUR B_EXPORT_FUNC void
b_run_loop_deallocate(
    B_TRANSFER struct B_RunLoop *run_loop) {
  B_PRECONDITION(run_loop);

  run_loop->vtable.deallocate(run_loop);
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_add_function(
    B_BORROW struct B_RunLoop *run_loop,
    B_RunLoopFunction *callback,
    B_RunLoopFunction *cancel,
    B_BORROW void const *callback_data,
    size_t callback_data_size,
    B_BORROW struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel);
  B_OUT_PARAMETER(e);

  if (!run_loop->vtable.add_function(
      run_loop,
      callback,
      cancel,
      callback_data,
      callback_data_size,
      e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_add_process_id(
    B_BORROW struct B_RunLoop *run_loop,
    B_ProcessID pid,
    B_RunLoopProcessFunction *callback,
    B_RunLoopFunction *cancel,
    B_BORROW void const *callback_data,
    size_t callback_data_size,
    B_BORROW struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel);
  B_OUT_PARAMETER(e);

  if (!run_loop->vtable.add_process_id(
      run_loop,
      pid,
      callback,
      cancel,
      callback_data,
      callback_data_size,
      e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_run(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  if (!run_loop->vtable.run(run_loop, e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_run_loop_stop(
    B_BORROW struct B_RunLoop *run_loop,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_OUT_PARAMETER(e);

  if (!run_loop->vtable.stop(run_loop, e)) {
    return false;
  }
  return true;
}
