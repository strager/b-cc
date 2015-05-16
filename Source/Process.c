#include <B/Private/Assertions.h>
#include <B/Process.h>

B_WUR B_EXPORT_FUNC bool
b_process_exit_status_equal(
    struct B_ProcessExitStatus const *a,
    struct B_ProcessExitStatus const *b) {
  B_PRECONDITION(a);
  B_PRECONDITION(b);

  if (a->type != b->type) {
    return false;
  }
  switch (a->type) {
  case B_PROCESS_EXIT_STATUS_SIGNAL:
    return a->u.signal.signal_number
      == b->u.signal.signal_number;
  case B_PROCESS_EXIT_STATUS_CODE:
    return a->u.code.exit_code == b->u.code.exit_code;
  case B_PROCESS_EXIT_STATUS_EXCEPTION:
    return a->u.exception.code == b->u.exception.code;
  default:
    B_BUG();
  }
}
