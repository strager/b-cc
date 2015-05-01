#pragma once

#include <stdint.h>

typedef int64_t B_ProcessID;

enum B_ProcessExitStatusType {
  // A POSIX signal (e.g. SIGTERM) terminated a process.
  B_PROCESS_EXIT_STATUS_SIGNAL = 1,

  // The process exited normally with an exit code.
  B_PROCESS_EXIT_STATUS_CODE = 2,

  // A Win32 exception in the process (e.g.
  // EXCEPTION_ACCESS_VIOLATION) terminated the process.
  B_PROCESS_EXIT_STATUS_EXCEPTION = 3,
};

struct B_ProcessExitStatusSignal {
  int signal_number;
};

struct B_ProcessExitStatusCode {
  int64_t exit_code;
};

struct B_ProcessExitStatusException {
  uint32_t code;
};

struct B_ProcessExitStatus {
  enum B_ProcessExitStatusType type;
  union {
    // B_PROCESS_EXIT_STATUS_SIGNAL
    struct B_ProcessExitStatusSignal signal;

    // B_PROCESS_EXIT_STATUS_CODE
    struct B_ProcessExitStatusCode code;

    // B_PROCESS_EXIT_STATUS_EXCEPTION
    struct B_ProcessExitStatusException exception;
  } u;
};

#if defined(__cplusplus)
# include <stdlib.h>

static inline bool
operator==(
    struct B_ProcessExitStatus a,
    struct B_ProcessExitStatus b) {
  if (a.type != b.type) {
    return false;
  }

  switch (a.type) {
  case B_PROCESS_EXIT_STATUS_SIGNAL:
    return a.u.signal.signal_number
      == b.u.signal.signal_number;
  case B_PROCESS_EXIT_STATUS_CODE:
    return a.u.code.exit_code == b.u.code.exit_code;
  case B_PROCESS_EXIT_STATUS_EXCEPTION:
    return a.u.exception.code == b.u.exception.code;
  default:
    abort();
  }
}
#endif
