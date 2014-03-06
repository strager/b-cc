#ifndef B_HEADER_GUARD_F8703BF6_2517_4ADC_98EB_163A2DFFB549
#define B_HEADER_GUARD_F8703BF6_2517_4ADC_98EB_163A2DFFB549

#include <B/Base.h>

#include <stddef.h>
#include <stdint.h>

struct B_Error;
struct B_ErrorHandler;

// A run loop which controls processes.
//
// Normal flow:
//
// * Allocate a ProcessLoop with b_process_loop_allocate.
// * Call b_process_loop_run_async_unsafe to run the
//   ProcessLoop in a background thread.
// * Run a few commands using b_process_loop_exec on the
//   ProcessLoop.
// * When each command exits, exit_callback given to
//   b_process_loop_exec will be called on another thread.
//   Synchronize to feed this information to your main
//   application.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_ProcessLoop;

typedef B_FUNC
B_ProcessExitCallback(
        int exit_code,
        void *opaque,
        struct B_ErrorHandler const *);

typedef B_FUNC
B_ProcessErrorCallback(
        struct B_Error,
        void *opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
extern "C" {
#endif

// Allocates a ProcessLoop with no processes.  Processes can
// be run on the returned ProcessLoop using
// b_process_loop_exec.  concurrent_process_limit is the
// number of processes which can run at the same time.  If
// concurrent_process_limit is zero, there is no artifically
// imposed limit.
B_EXPORT_FUNC
b_process_loop_allocate(
        size_t concurrent_process_limit,
        B_OUTPTR struct B_ProcessLoop **,
        struct B_ErrorHandler const *);

// Deallocates a ProcessLoop.  Kills running processes as if
// b_process_loop_killed was called, and stops callers
// blocked in b_process_loop_run_sync.
B_EXPORT_FUNC
b_process_loop_deallocate(
        B_TRANSFER struct B_ProcessLoop *,
        uint64_t process_force_kill_timeout_picoseconds,
        struct B_ErrorHandler const *);

// Runs a ProcessLoop in another thread (via
// b_process_loop_run_sync).  Non-blocking.
//
// Errors which occur in the spawned thread will be handled
// by the default error handler.
// FIXME(strager): What a bummer!
//
// When b_process_loop_stop is called, the thread is
// terminated.  
B_EXPORT_FUNC
b_process_loop_run_async_unsafe(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

// Runs a ProcessLoop in the current thread.  May only be
// called by one thread at a time.  Non-blocking.
//
// When b_process_loop_stop is called, this function returns
// successfully.
B_EXPORT_FUNC
b_process_loop_run_sync(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

// Kills[1] each process running in the ProcessLoop.  May be
// called while b_process_loop_run_sync is running.
// Blocking.
//
// Processes are first soft-killed[1].  After
// process_force_kill_after_picoseconds, if non-zero, if a
// process has not died, an ETIMEDOUT error is reported.  If
// B_ERROR_RETRY is requested, the process is soft-killed[1]
// again.  If B_ERROR_IGNORE is requested, the failure to
// kill the process is ignored.  If B_ERROR_ABORT is
// requested, the process is hard-killed[2], and
// b_process_loop_stop waits
// process_force_kill_after_picoseconds for the function to
// die.  In any case, b_process_loop_stop returns true if a
// process fails to be killed.
//
// [1] On POSIX-like systems, a process is soft-killed by
//     sending it the SIGTERM signal once.  On Win32
//     systems, a process is soft-killed by calling
//     ExitProcess in the process' address space.
//
// [2] On POSIX-like systems, a process is hard-killed by
//     sending it the SIGKILL signal once.  On Win32
//     systems, a process is hard-killed by calling
//     TerminateProcess.
B_EXPORT_FUNC
b_process_loop_kill(
        struct B_ProcessLoop *,
        uint64_t process_force_kill_after_picoseconds,
        struct B_ErrorHandler const *);

// Causes b_process_loop_run_sync to return.  Processes
// running in the ProcessLoop continue to run.
B_EXPORT_FUNC
b_process_loop_stop(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

// Starts a process in a ProcessLoop.  Callbacks are called
// on the thread in which b_process_loop_run_sync or
// b_process_loop_kill is called.
//
// args is a NULL-terminated list of \0-terminated
// system-encoded strings.
//
// FIXME(strager): We should expose a Unicode API for
// Windows.
B_EXPORT_FUNC
b_process_loop_exec(
        struct B_ProcessLoop *,
        char const *const *args,
        B_ProcessExitCallback *,
        B_ProcessErrorCallback *,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/CXX.h>

struct B_ProcessLoopDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(
            B_ProcessLoop *loop) {
        (void) b_process_loop_deallocate(
            loop,
            0,  // process_force_kill_timeout_picoseconds
            this->error_handler);
    }
};
#endif

#endif
