#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/Private/Config.h>
#include <B/RunLoop.h>

#include <errno.h>

#if B_CONFIG_POSIX_SPAWN
# include <spawn.h>
#endif

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

#if B_CONFIG_POSIX_SPAWN
static char **
b_environ_(
    void) {
  extern char **environ;
  return environ;
}
#endif

B_WUR B_EXPORT_FUNC bool
b_run_loop_exec_basic(
    B_BORROW struct B_RunLoop *run_loop,
    B_BORROW char const *const *command_args,
    B_RunLoopProcessFunction *callback,
    B_RunLoopFunction *cancel_callback,
    B_BORROW void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(run_loop);
  B_PRECONDITION(command_args);
  B_PRECONDITION(command_args[0]);
  B_PRECONDITION(callback);
  B_PRECONDITION(cancel_callback);
  B_OUT_PARAMETER(e);

#if B_CONFIG_POSIX_SPAWN
  pid_t pid;
  int rc = posix_spawnp(
    &pid,
    command_args[0],
    NULL,
    NULL,
    // FIXME(strager): This cast looks like a bug.
    (char *const *) command_args,
    b_environ_());
  if (rc != 0) {
    *e = (struct B_Error) {.posix_error = rc};
    return false;
  }
  if (!b_run_loop_add_process_id(
      run_loop,
      pid,
      callback,
      cancel_callback,
      callback_data,
      callback_data_size,
      e)) {
    // Should we kill the child or something?
    return false;
  }
  return true;
#else
# error "Unknown process start implementation"
#endif
}
