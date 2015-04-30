#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/RunLoop.h>

B_WUR B_EXPORT_FUNC bool
b_run_loop_allocate_preferred(
    B_OUT_TRANSFER struct B_RunLoop **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_RunLoop *run_loop;
  if (!b_run_loop_allocate_plain(&run_loop, e)) {
    return false;
  }
  *out = run_loop;
  return true;
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
  B_PRECONDITION(callback_data);
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
