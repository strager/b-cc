#pragma once

#include <B/Attributes.h>

#include <stdint.h>

struct B_ErrorHandler;

// A ProcessManager encapsulates the logic of handling child
// processes created with e.g. posix_spawn or CreateProcess.
//
// There are three components to ProcessManager: the
// ProcessManager itself, its ProcessControllerDelegate, and
// an attached ProcessController.
//
// The ProcessController has methods to register a callback
// to be called when a child process exits or a child
// process writes to a standard pipe (e.g. stdout or
// stderr). The user of a ProcessController can create child
// processes in any way it wishes. The user of a
// ProcessController is typically the AnswerCallback for a
// Question. For example, a C compiler process can be added
// to the ProcessController so an object file Question can
// be answered when the C compilation completes.
//
// The ProcessControllerDelegate takes requests from the
// ProcessController (process identifiers/handles and pipe
// file descriptors/handles) and installs them into an event
// loop. For example, a pipe descriptor/handle may be added
// to a select() FD_SET.
//
// A ProcessManager ties the ProcessController and
// ProcessControllerDelegate together, and provides an
// interface for dealing with platform-specific nuances.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_ProcessController;
struct B_ProcessControllerDelegate;
struct B_ProcessManager;

typedef int64_t B_ProcessID;

struct B_ProcessControllerDelegate {
  // Called when b_process_manager_register_process_id is
  // called.
  B_OPTIONAL B_WUR B_FUNC bool (*register_process_id)(
      struct B_ProcessControllerDelegate *,
      B_ProcessID,
      B_OUT struct B_Error *);

  // Must be non-NULL if register_process_process_id is
  // non-NULL.
  B_OPTIONAL B_WUR B_FUNC bool (*unregister_process_id)(
      struct B_ProcessControllerDelegate *,
      B_ProcessID,
      B_OUT struct B_Error *);

  // Called when b_process_manager_register_process_handle
  // is called.
  B_OPTIONAL B_WUR B_FUNC bool (*register_process_handle)(
      struct B_ProcessControllerDelegate *,
      void *,
      B_OUT struct B_Error *);

  // Must be non-NULL if register_process_handle is
  // non-NULL.
  B_OPTIONAL B_WUR B_FUNC bool (*unregister_process_handle)(
      struct B_ProcessControllerDelegate *,
      void *,
      B_OUT struct B_Error *);
};

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

typedef B_WUR B_FUNC bool
B_ProcessExitCallback(
    struct B_ProcessExitStatus const *,
    void *callback_opaque,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_process_manager_allocate(
    B_BORROW struct B_ProcessControllerDelegate *,
    B_OUT_TRANSFER struct B_ProcessManager **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_process_manager_deallocate(
    B_TRANSFER struct B_ProcessManager *,
    B_OUT struct B_Error *);

// Call when a registered process ID or process handle is
// signalled.
//
// This function is *not* signal safe and cannot be called
// from a signal (e.g. SIGCHLD) handler.
B_WUR B_EXPORT_FUNC bool
b_process_manager_check(
    B_BORROW struct B_ProcessManager *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_process_manager_get_controller(
    B_BORROW struct B_ProcessManager *,
    B_OUT_BORROW struct B_ProcessController **,
    B_OUT struct B_Error *);

// Registers a callback to be called when the given process
// exits. Accepts a process ID to register. Prefer
// b_process_controller_register_process_handle over this
// function where possible (e.g. on Windows).
B_WUR B_EXPORT_FUNC bool
b_process_controller_register_process_id(
    B_BORROW struct B_ProcessController *,
    B_ProcessID,
    B_TRANSFER B_ProcessExitCallback *callback,
    B_TRANSFER void *callback_opaque,
    B_OUT struct B_Error *);

// Registers a callback to be called when the given process
// exits. Accepts a process handle to register. Prefer
// this function over
// b_process_controller_register_process_id where possible
// (e.g. on Windows).
B_WUR B_EXPORT_FUNC bool
b_process_controller_register_process_handle(
    B_BORROW struct B_ProcessController *,
    B_BORROW void *process_handle,  // HANDLE
    B_TRANSFER B_ProcessExitCallback *callback,
    B_TRANSFER void *callback_opaque,
    B_OUT struct B_Error *);

// Spawns a process and calls the given callback when the
// process exits.
//
// args is a NULL-terminated list of \0-terminated
// system-encoded strings.
//
// FIXME(strager): We should expose a Unicode API for
// Windows.
B_WUR B_EXPORT_FUNC bool
b_process_controller_exec_basic(
    B_BORROW struct B_ProcessController *,
    B_BORROW char const *const *args,
    B_TRANSFER B_ProcessExitCallback *callback,
    B_TRANSFER void *callback_opaque,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif

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
