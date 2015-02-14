#ifndef B_HEADER_GUARD_CBFA240C_80E4_4B50_9847_CD2CEBF5389B
#define B_HEADER_GUARD_CBFA240C_80E4_4B50_9847_CD2CEBF5389B

#include <B/Base.h>

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
// stderr).  The user of a ProcessController can create
// child processes in any way it wishes.  The user of a
// ProcessController is typically the AnswerCallback for a
// Question.  For example, a C compiler process can be added
// to the ProcessController so an object file Question can
// be answered when the C compilation completes.
//
// The ProcessControllerDelegate takes requests from the
// ProcessController (process identifiers/handles and pipe
// file descriptors/handles) and installs them into an event
// loop.  For example, a pipe descriptor/handle may be added
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
    B_OPT B_FUNC (*register_process_id)(
            struct B_ProcessControllerDelegate *,
            B_ProcessID,
            struct B_ErrorHandler const *);

    // Must be non-NULL if register_process_process_id is
    // non-NULL.
    B_OPT B_FUNC (*unregister_process_id)(
            struct B_ProcessControllerDelegate *,
            B_ProcessID,
            struct B_ErrorHandler const *);

    // Called when b_process_manager_register_process_handle
    // is called.
    B_OPT B_FUNC (*register_process_handle)(
            struct B_ProcessControllerDelegate *,
            void *,
            struct B_ErrorHandler const *);

    // Must be non-NULL if register_process_handle is
    // non-NULL.
    B_OPT B_FUNC (*unregister_process_handle)(
            struct B_ProcessControllerDelegate *,
            void *,
            struct B_ErrorHandler const *);
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
    };
};

typedef B_FUNC
B_ProcessExitCallback(
        struct B_ProcessExitStatus const *,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_process_manager_allocate(
        B_BORROWED struct B_ProcessControllerDelegate *,
        B_OUTPTR struct B_ProcessManager **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_process_manager_deallocate(
        B_TRANSFER struct B_ProcessManager *,
        struct B_ErrorHandler const *);

// Call when a registered process ID or process handle is
// signalled.
//
// This function is *not* signal safe and cannot be called
// from a signal (e.g. SIGCHLD) handler.
B_EXPORT_FUNC
b_process_manager_check(
        struct B_ProcessManager *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_process_manager_get_controller(
        struct B_ProcessManager *,
        B_OUTPTR B_BORROWED struct B_ProcessController **,
        struct B_ErrorHandler const *);

// Registers a callback to be called when the given process
// exits.  Accepts a process ID to register.  Prefer
// b_process_controller_register_process_handle over this
// function where possible (e.g. on Windows).
B_EXPORT_FUNC
b_process_controller_register_process_id(
        struct B_ProcessController *,
        B_ProcessID,
        B_ProcessExitCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

// Registers a callback to be called when the given process
// exits.  Accepts a process handle to register.  Prefer
// this function over
// b_process_controller_register_process_id where possible
// (e.g. on Windows).
B_EXPORT_FUNC
b_process_controller_register_process_handle(
        struct B_ProcessController *,
        void *process_handle,  // HANDLE
        B_ProcessExitCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

// Spawns a process and calls the given callback when the
// process exits.
//
// args is a NULL-terminated list of \0-terminated
// system-encoded strings.
//
// FIXME(strager): We should expose a Unicode API for
// Windows.
B_EXPORT_FUNC
b_process_controller_exec_basic(
        struct B_ProcessController *,
        char const *const *args,
        B_ProcessExitCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Alloc.h>
# include <B/Assert.h>

static inline bool
operator==(
        B_ProcessExitStatus a,
        B_ProcessExitStatus b) {
    if (a.type != b.type) {
        return false;
    }

    switch (a.type) {
    case B_PROCESS_EXIT_STATUS_SIGNAL:
        return a.signal.signal_number
            == b.signal.signal_number;
    case B_PROCESS_EXIT_STATUS_CODE:
        return a.code.exit_code == b.code.exit_code;
    case B_PROCESS_EXIT_STATUS_EXCEPTION:
        return a.exception.code == b.exception.code;
    default:
        B_BUG();
    }
}

template<typename TExitCallback>
B_FUNC
b_process_controller_exec_basic(
        struct B_ProcessController *controller,
        char const *const *args,
        TExitCallback callback,
        struct B_ErrorHandler const *eh) {
    TExitCallback *heap_callback;
    if (!b_allocate(
            sizeof(*heap_callback),
            reinterpret_cast<void **>(&heap_callback),
            eh)) {
        return false;
    }
    new (heap_callback) TExitCallback(
        static_cast<TExitCallback &&>(callback));

    B_ProcessExitCallback *real_callback;
    real_callback = [](
            B_ProcessExitStatus const *exit_status,
            void *opaque,
            B_ErrorHandler const *eh) -> bool {
        auto heap_callback
            = static_cast<TExitCallback *>(opaque);
        bool ok = (*heap_callback)(exit_status, eh);
        heap_callback->~TExitCallback();
        (void) b_deallocate(heap_callback, eh);
        return ok;
    };

    if (!b_process_controller_exec_basic(
            controller,
            args,
            real_callback,
            heap_callback,
            eh)) {
        goto fail;
    }

    return true;

fail:
    if (heap_callback) {
        heap_callback->~TExitCallback();
        (void) b_deallocate(heap_callback, eh);
    }
    return false;
}
#endif

#endif
