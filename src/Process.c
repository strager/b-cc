// TODO(strager): We should be notified of and handle
// SIGCHLD signals in order to avoid race conditions.

#include <B/Config.h>

// If defined, always assume processes need to be enqueued
// (i.e. a running loop will start the process).  If not
// defined, processes can be spawned immediately.
//#define B_DEBUG_ALWAYS_ENQUEUE

// FIXME(strager): Must be included before other files for
// some reason.  Otherwise, sigemptyset is not defined.
#if defined(B_CONFIG_EPOLL)
# define _POSIX_SOURCE
# include <signal.h>
#endif

#if !defined(B_CONFIG_POSIX_SPAWN)
# error "posix_spawn support is assumed for this B_ProcessLoop implementation"
#endif
#if !defined(B_CONFIG_PTHREAD)
# error "pthread support is assumed for this B_ProcessLoop implementation"
#endif
#if !defined(B_CONFIG_SYSQUEUE)
# error "sys/queue.h support is assumed for this B_ProcessLoop implementation"
#endif

#if !defined(B_CONFIG_KQUEUE) && !defined(B_CONFIG_EPOLL)
# error "Only kqueue and epoll are implemented for B_ProcessLoop"
#endif

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Error.h>
#include <B/Log.h>
#include <B/Macro.h>
#include <B/Process.h>
#include <B/Thread.h>

#if defined(B_CONFIG_EPOLL)
# include <B/Misc.h>
#endif

#include <errno.h>
#include <string.h>
#include <sys/queue.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

#if defined(B_CONFIG_POSIX_SPAWN)
# include <spawn.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

#if defined(B_CONFIG_KQUEUE)
# include <sys/event.h>
#elif defined(B_CONFIG_EPOLL)
# include <stdio.h>
# include <stdlib.h>
# include <sys/epoll.h>
# include <sys/eventfd.h>
# include <sys/signalfd.h>
# include <sys/wait.h>
#endif

// FIXME(strager): We should use FreeBSD's excellent
// <sys/queue.h> implementation.  Linux does not have
// LIST_FOREACH or LIST_FOREACH_SAFE, for example.
#if !defined(LIST_FOREACH_SAFE)
# if !defined(__GLIBC__)
#  error "Missing LIST_FOREACH_SAFE"
# endif
# define LIST_FOREACH_SAFE(_var, _head, _name, _tmp_var) \
    for ( \
            (_var) = (_head)->lh_first; \
            (_var) && (((_tmp_var) = (_var)->_name.le_next), true); \
            (_var) = (_tmp_var))
#endif

#if !defined(LIST_FOREACH)
# if !defined(__GLIBC__)
#  error "Missing LIST_FOREACH"
# endif
# define LIST_FOREACH(_var, _head, _name) \
    for ( \
            (_var) = (_head)->lh_first; \
            (_var); \
            (_var) = (_var)->_name.le_next)
#endif

// FIXME(strager): Reporting errors while holding a lock is
// a bad idea!
#define B_ERROR_WHILE_LOCKED() B_BUG()

#if defined(B_CONFIG_POSIX_SPAWN)
// FIXME(strager)
# define B_INVALID_PID ((pid_t) -1)
#endif

enum {
    // The EVFILT_USER kevent ident for the kevent which is
    // signaled to wake up the loop.
    B_PROCESS_LOOP_KQUEUE_USER_IDENT = 1234,
};

enum LoopState_ {
    // The ProcessLoop is not running at all.
    B_PROCESS_LOOP_NOT_RUNNING,

    // The ProcessLoop is running and is waiting in a kevent
    // call.  It can be interrupted with
    // process_loop_interrupt_locked_.
    B_PROCESS_LOOP_POLLING,

    // The ProcessLoop is running but is not waiting in a
    // kevent call.
    B_PROCESS_LOOP_BUSY,
};

enum LoopRequest_ {
    // The running ProcessLoop should continue normal
    // operation.
    B_PROCESS_LOOP_CONTINUE,

    // The running ProcessLoop should stop and return to its
    // caller.
    B_PROCESS_LOOP_STOP,
};

struct B_ProcessLoop {
    pthread_mutex_t lock;

    // All following fields are locked by lock.

#if defined(B_CONFIG_KQUEUE)
    int queue;  // kqueue()
#elif defined(B_CONFIG_EPOLL)
    int epoll;  // epoll_create()
    int epoll_interrupt;  // eventfd()
    int epoll_sigchld;  // signalfd(SIGCHLD)
#endif

    enum B_ProcessConfiguration configuration;

    size_t concurrent_process_limit;

    enum LoopState_ loop_state;
    // Signaled when loop_state changes.
    pthread_cond_t loop_state_cond;

    enum LoopRequest_ loop_request;

    // FIXME(strager)
    LIST_HEAD(foo, ProcessInfo_) processes;
};

struct ProcessInfo_ {
    B_ProcessExitCallback *B_CONST_STRUCT_MEMBER exit_callback;
    B_ProcessErrorCallback *B_CONST_STRUCT_MEMBER error_callback;
    void *B_CONST_STRUCT_MEMBER callback_opaque;
    pid_t pid;

    // Locked by owning ProcessLoop::lock.  If non-NULL,
    // this process hasn't yet started; it is queued for
    // start.
    B_OPT char const *const *args;

    // Locked by owning ProcessLoop::lock.
    LIST_ENTRY(ProcessInfo_) link;
};

// See NOTE[loop_run_async synchronization].
struct ProcessLoopRunAsyncClosure_ {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct B_ProcessLoop *loop;
    bool running;
    bool errored;
    struct B_Error error;
};

// Duplicates args such that a call to b_deallocate on the
// output pointer will deallocate the entire args array,
// including strings.
static B_FUNC
dup_args_(
        char const *const *args,
        B_OUTPTR char const *const **,
        struct B_ErrorHandler const *);

static B_FUNC
create_async_thread_(
        struct ProcessLoopRunAsyncClosure_ *,
        struct B_ErrorHandler const *);

static void *
run_async_thread_(
        void *opaque);

static B_FUNC
run_sync_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

static B_FUNC
spawn_process_(
        char const *const *args,
        B_OUT pid_t *,
        struct B_ErrorHandler const *);

static B_FUNC
process_loop_exec_now_locked_(
        struct B_ProcessLoop *,
        struct ProcessInfo_ *,
        char const *const *args,
        struct B_ErrorHandler const *);

static B_FUNC
process_loop_exec_later_locked_(
        struct B_ProcessLoop *,
        struct ProcessInfo_ *,
        char const *const *args,
        struct B_ErrorHandler const *);

static bool
process_loop_can_exec_one_more_locked_(
        struct B_ProcessLoop *);

// Spawns processes until either the queue is empty or
// concurrent_process_limit is reached.
static B_FUNC
process_loop_fill_process_list_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

static B_FUNC
process_loop_interrupt_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

static B_FUNC
process_loop_stop_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

static B_FUNC
process_loop_wait_for_stop_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

static B_FUNC
set_process_loop_state_locked_(
        struct B_ProcessLoop *,
        enum LoopState_ loop_state,
        struct B_ErrorHandler const *);

static bool
should_stop_loop_locked_(
        struct B_ProcessLoop *);

static bool
process_is_queued_locked_(
        struct ProcessInfo_ const *);

static size_t
running_process_count_locked_(
        struct B_ProcessLoop *);

#if defined(B_CONFIG_KQUEUE)
static B_FUNC
handle_event_locked_(
        struct B_ProcessLoop *,
        struct kevent const *,
        struct B_ErrorHandler const *);
#endif

#if defined(B_CONFIG_EPOLL)
static B_FUNC
handle_event_locked_(
        struct B_ProcessLoop *,
        struct epoll_event const *,
        struct B_ErrorHandler const *);

static B_FUNC
check_processes_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);
#endif

static B_FUNC
process_exited_locked_(
        struct B_ProcessLoop *,
        struct ProcessInfo_ *,
        int exit_status,
        struct B_ErrorHandler const *);

#if defined(B_CONFIG_EPOLL)
static void
check_sigprocmask_locked_(
        struct B_ProcessLoop *);
#endif

B_EXPORT enum B_ProcessConfiguration
b_process_auto_configuration_unsafe(void) {
#if defined(B_CONFIG_KQUEUE)
    return B_PROCESS_CONFIGURATION_KQUEUE;
#elif defined(B_CONFIG_EPOLL)
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGCHLD);
    int rc = sigprocmask(SIG_BLOCK, &sigmask, NULL);
    B_ASSERT(rc == 0);
    return B_PROCESS_CONFIGURATION_SIGCHLD_MASKED;
#else
# error "Unknown B_ProcessLoop configuration"
#endif
}

B_EXPORT_FUNC
b_process_loop_allocate(
        size_t concurrent_process_limit,
        enum B_ProcessConfiguration configuration,
        B_OUTPTR struct B_ProcessLoop **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    int rc;
    struct B_ProcessLoop *loop = NULL;
#if defined(B_CONFIG_KQUEUE)
    int queue = -1;
#elif defined(B_CONFIG_EPOLL)
    int epoll = -1;
    int epoll_interrupt = -1;
    int epoll_sigchld = -1;
#endif

    switch (configuration) {
#if 0
    case B_PROCESS_CONFIGURATION_WIN32:
#endif
#if defined(B_CONFIG_KQUEUE)
    case B_PROCESS_CONFIGURATION_KQUEUE:
#endif
#if defined(B_CONFIG_EPOLL)
    case B_PROCESS_CONFIGURATION_SIGCHLD_MASKED:
#endif
#if defined(B_CONFIG_EPOLL) || defined(B_CONFIG_KQUEUE)
    case B_PROCESS_CONFIGURATION_SIGCHLD_CALL:
#endif
        break;
    default:
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            EINVAL,
            "configuration");
        return false;
    }

    if (configuration
            == B_PROCESS_CONFIGURATION_SIGCHLD_CALL) {
        // TODO(strager)
        B_NYI();
    }

    if (configuration
            == B_PROCESS_CONFIGURATION_KQUEUE) {
        // TODO(strager): Technically not supported, but we
        // get away with lack of support for now.
    }

    if (!b_allocate(sizeof(*loop), (void **) &loop, eh)) {
        goto fail;
    }

#if defined(B_CONFIG_KQUEUE)
retry_kqueue:
    queue = kqueue();
    if (queue == -1) {
        switch (B_RAISE_ERRNO_ERROR(eh, errno, "kqueue")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        case B_ERROR_RETRY:
            goto retry_kqueue;
        }
    }
#elif defined(B_CONFIG_EPOLL)
retry_epoll_create:
    epoll = epoll_create1(0);
    if (epoll == -1) {
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                errno,
                "epoll_create")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        case B_ERROR_RETRY:
            goto retry_epoll_create;
        }
    }

retry_eventfd:
    epoll_interrupt = eventfd(0, 0);
    if (epoll_interrupt == -1) {
        switch (B_RAISE_ERRNO_ERROR(eh, errno, "eventfd")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        case B_ERROR_RETRY:
            goto retry_eventfd;
        }
    }
    if (!b_epoll_ctl_fd(
            epoll,
            EPOLL_CTL_ADD,
            epoll_interrupt,
            EPOLLIN,
            eh)) {
        goto fail;
    }

retry_signalfd:
    {
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGCHLD);
        epoll_sigchld = signalfd(
            -1,
            &sigmask,
            SFD_NONBLOCK);
        if (epoll_interrupt == -1) {
            switch (B_RAISE_ERRNO_ERROR(
                    eh,
                    errno,
                    "signalfd")) {
            case B_ERROR_ABORT:
            case B_ERROR_IGNORE:
                goto fail;
            case B_ERROR_RETRY:
                goto retry_signalfd;
            }
        }
    }
    if (!b_epoll_ctl_fd(
            epoll,
            EPOLL_CTL_ADD,
            epoll_sigchld,
            EPOLLIN,
            eh)) {
        goto fail;
    }
#endif

    *loop = (struct B_ProcessLoop) {
        .lock = PTHREAD_MUTEX_INITIALIZER,
#if defined(B_CONFIG_KQUEUE)
        .queue = queue,
#elif defined(B_CONFIG_EPOLL)
        .epoll = epoll,
        .epoll_interrupt = epoll_interrupt,
        .epoll_sigchld = epoll_sigchld,
#endif
        .configuration = configuration,
        .concurrent_process_limit
            = concurrent_process_limit,
        .loop_state = B_PROCESS_LOOP_NOT_RUNNING,
        .loop_state_cond = PTHREAD_COND_INITIALIZER,
        .loop_request = B_PROCESS_LOOP_CONTINUE,
        .processes
            = LIST_HEAD_INITIALIZER(&loop->processes),
    };

retry_mutex_init:
    rc = pthread_mutex_init(&loop->lock, NULL);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                rc,
                "pthread_mutex_init")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        case B_ERROR_RETRY:
            goto retry_mutex_init;
        }
    }

    B_LOG(B_DEBUG, "Created loop %p", loop);

    *out = loop;
    return true;

fail:
#if defined(B_CONFIG_KQUEUE)
    if (queue != -1) {
        // TODO(strager): Error reporting.
        (void) close(queue);
    }
#elif defined(B_CONFIG_EPOLL)
    if (epoll != -1) {
        // TODO(strager): Error reporting.
        (void) close(epoll);
    }
    if (epoll_interrupt != -1) {
        // TODO(strager): Error reporting.
        (void) close(epoll_interrupt);
    }
    if (epoll_sigchld != -1) {
        // TODO(strager): Error reporting.
        (void) close(epoll_sigchld);
    }
#endif
    if (loop) {
        (void) b_deallocate(loop, eh);
    }
    return false;
}

B_EXPORT_FUNC
b_process_loop_deallocate(
        B_TRANSFER struct B_ProcessLoop *loop,
        uint64_t process_force_kill_timeout_picoseconds,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);

    B_LOG(B_DEBUG, "Loop %p deallocating", loop);

    // TODO(strager): Error reporting.
    int rc;

    bool ok = true;

    if (!B_MUTEX_LOCK(loop->lock, eh)) return false;
    {
        // TODO(strager): Kill processes.
        (void) process_force_kill_timeout_picoseconds;

        // Stop the loop.
        ok = process_loop_stop_locked_(loop, eh) && ok;
        if (!ok) B_ERROR_WHILE_LOCKED();
        ok = process_loop_wait_for_stop_locked_(loop, eh)
            && ok;
        if (!ok) B_ERROR_WHILE_LOCKED();

#if defined(B_CONFIG_KQUEUE)
        close(loop->queue);
        loop->queue = -1;
#elif defined(B_CONFIG_EPOLL)
        close(loop->epoll);
        loop->epoll = -1;
        close(loop->epoll_interrupt);
        loop->epoll_interrupt = -1;
        close(loop->epoll_sigchld);
        loop->epoll_sigchld = -1;
#endif

        B_ASSERT(loop->loop_state
            == B_PROCESS_LOOP_NOT_RUNNING);
    }
    B_MUTEX_MUST_UNLOCK(loop->lock, eh);

    rc = pthread_mutex_destroy(&loop->lock);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            rc,
            "pthread_mutex_destroy");
        ok = false;
    }

    ok = b_deallocate(loop, eh) && ok;
    return ok;
}

B_EXPORT_FUNC
b_process_loop_run_async_unsafe(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);

    // TODO(strager): Proper error handling.
    int rc;

    // NOTE[loop_run_async synchronization]: We need to
    // ensure run_async_thread_ has called run_sync_locked_.
    // Otherwise, b_process_loop_deallocate could be called
    // before our thread gets scheduled, and our thread
    // would have a dangling ProcessLoop pointer.  Not good.
    struct ProcessLoopRunAsyncClosure_ closure = {
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .loop = loop,
        .running = false,
        .errored = false,
        // .error
    };

    if (!create_async_thread_(&closure, eh)) {
        return false;
    }

    bool ok;

    if (!B_MUTEX_LOCK(closure.lock, eh)) return false;
    {
        while (!closure.running && !closure.errored) {
            rc = pthread_cond_wait(
                &closure.cond,
                &closure.lock);
            B_ASSERT(rc == 0);
        }
        B_ASSERT(closure.running != closure.errored);

        if (closure.running) {
            // NOTE(strager): loop->lock (likely) held.
            ok = true;
        } else if (closure.errored) {
            // NOTE(strager): Spawned thread is dying or
            // dead.
            B_ERROR_WHILE_LOCKED();
            switch (b_raise_error(eh, closure.error)) {
            case B_ERROR_ABORT:
            case B_ERROR_IGNORE:
                ok = false;
                break;
            case B_ERROR_RETRY:
                // TODO(strager)
                ok = false;
                break;
            }
        } else {
            B_BUG();
            ok = true;
        }
    }
    B_MUTEX_MUST_UNLOCK(closure.lock, eh);

    return ok;
}

B_EXPORT_FUNC
b_process_loop_run_sync(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);

    if (!B_MUTEX_LOCK(loop->lock, eh)) {
        return false;
    }

    bool ok = run_sync_locked_(loop, eh);
    if (!ok) B_ERROR_WHILE_LOCKED();

    B_MUTEX_MUST_UNLOCK(loop->lock, eh);

    return ok;
}

B_EXPORT_FUNC
b_process_loop_kill(
        B_TRANSFER struct B_ProcessLoop *loop,
        uint64_t process_force_kill_after_picoseconds,
        struct B_ErrorHandler const *eh) {
    (void) loop;
    (void) process_force_kill_after_picoseconds;
    (void) eh;
    B_NYI();
}

B_EXPORT_FUNC
b_process_loop_stop(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);

    B_LOG(B_DEBUG, "Stopping loop %p", loop);

    if (!B_MUTEX_LOCK(loop->lock, eh)) return false;

    bool ok = process_loop_stop_locked_(loop, eh);
    if (!ok) B_ERROR_WHILE_LOCKED();

    B_MUTEX_MUST_UNLOCK(loop->lock, eh);

    return ok;
}

B_EXPORT_FUNC
b_process_loop_exec(
        struct B_ProcessLoop *loop,
        char const *const *args,
        B_ProcessExitCallback *exit_callback,
        B_ProcessErrorCallback *error_callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);
    B_CHECK_PRECONDITION(eh, args);
    B_CHECK_PRECONDITION(eh, exit_callback);
    B_CHECK_PRECONDITION(eh, error_callback);

    {
        // Log the command being run.
        bool acquired_lock = b_log_lock();

        b_log_format_raw_locked("$");
        for (char const *const *arg = args; *arg; ++arg) {
            b_log_format_raw_locked(" %s", *arg);
        }
        b_log_format_raw_locked("\n");

        if (acquired_lock) {
            b_log_unlock();
        }
    }

    // * Notify the loop, if running, that the process list
    // has changed.
    // * Spawn the child process.
    // * Add the child process to the loop's process list.
    // * Add the child process to the kqueue.
    //
    // These steps are performed atomically (under the
    // ProcessLoop's lock).

    struct ProcessInfo_ *proc;
    if (!b_allocate(sizeof(*proc), (void **) &proc, eh)) {
        return false;
    }

    if (!B_MUTEX_LOCK(loop->lock, eh)) {
        (void) b_deallocate(proc, eh);
        return false;
    }

    *proc = (struct ProcessInfo_) {
        .exit_callback = exit_callback,
        .error_callback = error_callback,
        .callback_opaque = callback_opaque,
        .args = NULL,
        .pid = B_INVALID_PID,
        // .link
    };
#if defined(B_DEBUG_ALWAYS_ENQUEUE)
    bool exec_now = false;
    (void) running_process_count_locked_;
#else
    bool exec_now
        = process_loop_can_exec_one_more_locked_(loop);
#endif
    bool ok = exec_now
        ? process_loop_exec_now_locked_(
                loop,
                proc,
                args,
                eh)
        : process_loop_exec_later_locked_(
                loop,
                proc,
                args,
                eh);
    if (!ok) {
        B_ERROR_WHILE_LOCKED();
        (void) b_deallocate(proc, eh);
        goto done;
    }

    LIST_INSERT_HEAD(&loop->processes, proc, link);

done:
    B_MUTEX_MUST_UNLOCK(loop->lock, eh);
    return true;
}

static B_FUNC
dup_args_(
        char const *const *args,
        B_OUTPTR char const *const **out,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(args);
    B_ASSERT(out);

    size_t arg_count = 0;    // Excludes NULL terminator.
    size_t args_length = 0;  // Excludes \0 terminators.
    for (char const *const *p = args; *p; ++p) {
        arg_count += 1;
        args_length += strlen(*p);
    }

    // Layout:
    // 0000: 0008  // Pointer to "echo".
    // 0002: 000D  // Pointer to "hello".
    // 0004: 0013  // Pointer to "world".
    // 0006: 0000  // NULL terminator for args.
    // 0008: "echo\0"
    // 000D: "hello\0"
    // 0013: "world\0"
    // 0019: <end of buffer>
    size_t arg_pointers_size
        = sizeof(char const *) * (arg_count + 1);
    size_t arg_strings_size = args_length + arg_count;
    size_t arg_buffer_size
        = arg_pointers_size + arg_strings_size;
    char const *const *args_buffer;
    if (!b_allocate(
            arg_buffer_size,
            (void **) &args_buffer,
            eh)) {
        return false;
    }
    void *args_buffer_end
        = (char *) args_buffer + arg_buffer_size;

    // Copy arg pointers.
    char const **out_arg_pointer
        = (char const **) args_buffer;
    for (char const *const *p = args; *p; ++p) {
        *out_arg_pointer++ = *p;
    }
    *out_arg_pointer++ = NULL;
    B_ASSERT((char *) out_arg_pointer
        == (char *) args_buffer + arg_pointers_size);

    // Copy arg strings.
    char *out_arg_string = (char *) out_arg_pointer;
    for (char const *const *p = args; *p; ++p) {
        size_t p_size = strlen(*p) + 1;
        B_ASSERT(out_arg_string + p_size
            <= (char *) args_buffer_end);
        memcpy(out_arg_string, *p, p_size);
        B_ASSERT(out_arg_string[p_size - 1] == '\0');
        out_arg_string += p_size;
    }
    B_ASSERT(out_arg_string == (char *) args_buffer_end);

    *out = args_buffer;
    return true;
}

static B_FUNC
create_async_thread_(
        struct ProcessLoopRunAsyncClosure_ *closure,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(closure);

    // TODO(strager): Proper error handling.
    int rc;

    pthread_attr_t thread_attr;
    rc = pthread_attr_init(&thread_attr);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
                eh,
                rc,
                "pthread_attr_init");
        return false;
    }

    rc = pthread_attr_setdetachstate(
            &thread_attr,
            PTHREAD_CREATE_DETACHED);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
                eh,
                rc,
                "pthread_attr_setdetachstate");
        return false;
    }

    pthread_t thread;
    rc = pthread_create(
            &thread,
            &thread_attr,
            run_async_thread_,
            closure);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
                eh,
                rc,
                "pthread_create");
        return false;
    }
    (void) thread;

    return true;
}

static void *
run_async_thread_(
        void *opaque) {
    // TODO(strager): Proper error handling.
    int rc;

    // NOTE(strager): We do not have an error handler here,
    // so we should try our best to report errors to the
    // spawning thread via closure->error.

    // See NOTE[loop_run_async synchronization].
    struct ProcessLoopRunAsyncClosure_ *closure = opaque;
    B_ASSERT(closure);

    if (!B_MUTEX_LOCK(closure->lock, NULL)) {
        B_BUG();
    }

    struct B_ProcessLoop *loop = closure->loop;
    B_ASSERT(loop);
    rc = pthread_mutex_lock(&loop->lock);
    if (rc != 0) {
        closure->errored = true;
        closure->error.errno_value = rc;
        rc = pthread_cond_signal(&closure->cond);
        B_ASSERT(rc == 0);
        B_MUTEX_MUST_UNLOCK(closure->lock, NULL);
        return NULL;
    }

    closure->running = true;
    rc = pthread_cond_signal(&closure->cond);
    B_ASSERT(rc == 0);
    B_MUTEX_MUST_UNLOCK(closure->lock, NULL);

    (void) run_sync_locked_(loop, NULL);

    B_MUTEX_MUST_UNLOCK(loop->lock, NULL);

    return NULL;
}

static B_FUNC
run_sync_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    int rc;

    B_LOG(B_DEBUG, "Running loop %p", loop);

    B_ASSERT(loop->loop_state
        == B_PROCESS_LOOP_NOT_RUNNING);
    while (!should_stop_loop_locked_(loop)) {
        if (!set_process_loop_state_locked_(
                loop,
                B_PROCESS_LOOP_BUSY,
                eh)) {
            B_ERROR_WHILE_LOCKED();
            goto fail_locked;
        }

        if (!process_loop_fill_process_list_locked_(
                loop,
                eh)) {
            // Ignore.
        }

#if defined(B_CONFIG_EPOLL)
        if (!check_processes_locked_(loop, eh)) {
            // Ignore.
        }
#endif

        if (should_stop_loop_locked_(loop)) {
            break;
        }

        if (!set_process_loop_state_locked_(
                loop,
                B_PROCESS_LOOP_POLLING,
                eh)) {
            B_ERROR_WHILE_LOCKED();
            goto fail_locked;
        }

#if defined(B_CONFIG_EPOLL)
        check_sigprocmask_locked_(loop);
#endif

#if defined(B_CONFIG_KQUEUE)
        int queue = loop->queue;
#elif defined(B_CONFIG_EPOLL)
        int epoll = loop->epoll;
#endif

        size_t events_received;

        bool ok;
        enum B_ErrorHandlerResult error_result;

        // Poll for events, unlocked.  queue should not be
        // closed because loop->loop_state ==
        // B_PROCESS_LOOP_POLLING.
        // FIXME(strager): See NOTE[event buffering].
        size_t const events_count = 1;
#if defined(B_CONFIG_KQUEUE)
        struct kevent events[events_count];
#elif defined(B_CONFIG_EPOLL)
        struct epoll_event events[events_count];
#endif
        rc = pthread_mutex_unlock(&loop->lock);
        B_ASSERT(rc == 0);
        {  // Unlocked block.
#if defined(B_CONFIG_KQUEUE)
            rc = kevent(
                queue,
                NULL,
                0,
                events,
                events_count,
                NULL);
            if (rc == -1) {
                // NOTE(strager): We don't want to report an
                // error while holding the lock.
                error_result = B_RAISE_ERRNO_ERROR(
                    eh,
                    errno,
                    "kevent");
                ok = false;
            } else {
                ok = true;
                B_ASSERT(rc >= 0);
                events_received = rc;
                B_ASSERT(events_received <= events_count);
            }
#elif defined(B_CONFIG_EPOLL)
            rc = epoll_wait(
                epoll,
                events,
                events_count,
                -1);
            if (rc == -1) {
                // NOTE(strager): We don't want to report an
                // error while holding the lock.
                error_result = B_RAISE_ERRNO_ERROR(
                    eh,
                    errno,
                    "epoll_pwait");
                ok = false;
            } else {
                ok = true;
                B_ASSERT(rc >= 0);
                events_received = rc;
                B_ASSERT(events_received <= events_count);
            }
#endif
        }
        if (!B_MUTEX_LOCK(loop->lock, eh)) {
            B_BUG();  // FIXME(strager)
        }

        if (!ok) {
            switch (error_result) {
            case B_ERROR_ABORT:
                goto fail_locked;
            case B_ERROR_RETRY:
            case B_ERROR_IGNORE:
                // Retrying and ignoring mean the same thing
                // here.  We should check if
                // B_PROCESS_LOOP_STOP was requested instead
                // of blindly calling kevent again.
                continue;
            }
        }

        if (should_stop_loop_locked_(loop)) {
            // FIXME(strager): What about events?  Should we
            // re-add them to the kqueue somehow?  Should we
            // process them?
            break;
        }

        // Handle events.
        if (!set_process_loop_state_locked_(
                loop,
                B_PROCESS_LOOP_BUSY,
                eh)) {
            B_ERROR_WHILE_LOCKED();
            goto fail_locked;
        }

        for (size_t i = 0; i < events_received; ++i) {
            ok = handle_event_locked_(loop, &events[i], eh)
                && ok;
            if (should_stop_loop_locked_(loop)) {
                if (i != events_received - 1) {
                    // NOTE[event buffering]: We need to
                    // buffer events.
                    B_BUG();
                }
                break;
            }
        }
        if (!ok) {
            goto fail_locked;
        }
    }
    B_ASSERT(should_stop_loop_locked_(loop));
    if (!set_process_loop_state_locked_(
            loop,
            B_PROCESS_LOOP_NOT_RUNNING,
            eh)) {
        B_ERROR_WHILE_LOCKED();
        // Fall through.
    }
    loop->loop_request = B_PROCESS_LOOP_CONTINUE;
    B_LOG(B_DEBUG, "Stopped loop %p", loop);
    return true;

fail_locked:
    B_LOG(B_DEBUG, "Stopped loop %p due to failure", loop);
    if (!set_process_loop_state_locked_(
            loop,
            B_PROCESS_LOOP_NOT_RUNNING,
            eh)) {
        B_ERROR_WHILE_LOCKED();
    }
    // Leave loop->loop_request untouched.
    return false;
}

static B_FUNC
spawn_process_(
        char const *const *args,
        B_OUT pid_t *out_pid,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(args);
    B_ASSERT(out_pid);

retry:;
    pid_t pid;
    posix_spawn_file_actions_t const *file_actions = NULL;
    posix_spawnattr_t const *attributes = NULL;
    char *const *envp = NULL;
    int rc = posix_spawnp(
        &pid,
        args[0],
        file_actions,
        attributes,
        // FIXME(strager): Cast looks like a bug!
        (char *const *) args,
        envp);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                errno,
                "posix_spawnp")) {
        case B_ERROR_IGNORE:
        case B_ERROR_ABORT:
            return false;
        case B_ERROR_RETRY:
            goto retry;
        }
    }

    *out_pid = pid;
    return true;
}

static B_FUNC
process_loop_exec_now_locked_(
        struct B_ProcessLoop *loop,
        struct ProcessInfo_ *proc,
        char const *const *args,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    B_ASSERT(proc);
    B_ASSERT(args);
    B_ASSERT(!proc->args);
    B_ASSERT(proc->pid == B_INVALID_PID);

    B_LOG(B_DEBUG, "Executing proc %p now", proc);

    // NOTE[late spawn]: Spawn as late as possible, so we
    // don't need to kill the process when unwinding due to
    // an error.
    pid_t pid;
    if (!spawn_process_(args, &pid, eh)) {
        B_ERROR_WHILE_LOCKED();
        (void) b_deallocate(proc, eh);
        return false;
    }
    proc->pid = pid;

#if defined(B_CONFIG_KQUEUE)
retry_kevent:;
    int rc = kevent(loop->queue, &(struct kevent) {
        .ident = pid,
        .filter = EVFILT_PROC,
        .flags = EV_ADD,
        .fflags = NOTE_EXIT | NOTE_EXITSTATUS,
        .data = (intptr_t) NULL,
        .udata = proc,
    }, 1, NULL, 0, NULL);
    if (rc == -1) {
        switch (B_RAISE_ERRNO_ERROR(eh, errno, "kevent")) {
        case B_ERROR_RETRY:
            // FIXME(strager)
            //B_ERROR_WHILE_LOCKED();
            goto retry_kevent;
        case B_ERROR_IGNORE:
        case B_ERROR_ABORT:
            // FIXME(strager): The process is perhaps
            // running.  Should we kill it?
            B_ERROR_WHILE_LOCKED();
            return false;
        }
    }
#elif defined(B_CONFIG_EPOLL)
    // epoll does not support waiting for child processes
    // like kqueue does.  Instead, we wait for SIGCHLD using
    // the epoll_sigchld descriptor.
    // FIXME(strager): Why do we need to interrupt here?
    // FIXME(strager): We shouldn't interrupt the loop if we
    // are calling this from within the loop.
    if (!process_loop_interrupt_locked_(loop, eh)) {
        // Ignore.
    }
#endif

    return true;
}

static B_FUNC
process_loop_exec_later_locked_(
        struct B_ProcessLoop *loop,
        struct ProcessInfo_ *proc,
        char const *const *args,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    B_ASSERT(proc);
    B_ASSERT(args);
    B_ASSERT(!proc->args);
    B_ASSERT(proc->pid == B_INVALID_PID);

    B_LOG(B_DEBUG, "Executing proc %p later", proc);

    if (!dup_args_(args, &proc->args, eh)) {
        B_ERROR_WHILE_LOCKED();
        return false;
    }
    if (!process_loop_interrupt_locked_(loop, eh)) {
        // Ignore.
    }
    return true;
}

static bool
process_loop_can_exec_one_more_locked_(
        struct B_ProcessLoop *loop) {
    return loop->concurrent_process_limit == 0
        || loop->concurrent_process_limit
            != running_process_count_locked_(loop);
}

static B_FUNC
process_loop_fill_process_list_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    bool ok = true;
    struct ProcessInfo_ *proc;
    LIST_FOREACH(
            proc,
            &loop->processes,
            link) {
        if (!process_loop_can_exec_one_more_locked_(loop)) {
            break;
        }
        if (process_is_queued_locked_(proc)) {
            char const *const *args = proc->args;
            proc->args = NULL;
            ok = process_loop_exec_now_locked_(
                loop,
                proc,
                args,
                eh) && ok;
            if (!b_deallocate((void *) args, eh)) {
                // FIXME(strager)
                B_BUG();
            }
        }
    }
    return ok;
}

static B_FUNC
process_loop_interrupt_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);

    int rc;

    // No need to interrupt if the process isn't running or
    // should be stopped anyway.
    if (loop->loop_state == B_PROCESS_LOOP_NOT_RUNNING) {
        return true;
    }

retry:
    B_LOG(B_DEBUG, "Interrupting loop %p", loop);
#if defined(B_CONFIG_KQUEUE)
    // Signal the running process loop with an EVFILT_USER.
    // NOTE(strager): Combining the two kevent calls does
    // not work; we must add the event *then* trigger it in
    // separate calls.
    rc = kevent(loop->queue, &(struct kevent) {
        .ident = B_PROCESS_LOOP_KQUEUE_USER_IDENT,
        .filter = EVFILT_USER,
        .flags = EV_ADD | EV_CLEAR | EV_ONESHOT,
        .fflags = 0,
        .data = (intptr_t) NULL,
        .udata = NULL,
    }, 1, NULL, 0, NULL);
    if (rc == -1) goto kevent_failed;
    rc = kevent(loop->queue, &(struct kevent) {
        .ident = B_PROCESS_LOOP_KQUEUE_USER_IDENT,
        .filter = EVFILT_USER,
        .flags = 0,
        .fflags = NOTE_TRIGGER,
        .data = (intptr_t) NULL,
        .udata = NULL,
    }, 1, NULL, 0, NULL);
    if (rc == -1) goto kevent_failed;
#else
    rc = eventfd_write(loop->epoll_interrupt, 1);
    if (rc == -1) {
        B_ERROR_WHILE_LOCKED();
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                errno,
                "eventfd_write")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        case B_ERROR_RETRY:
            goto retry;
        }
    }
#endif
    return true;

#if defined(B_CONFIG_KQUEUE)
kevent_failed:
    B_ASSERT(rc == -1);
    B_ERROR_WHILE_LOCKED();
    switch (B_RAISE_ERRNO_ERROR(eh, errno, "kevent")) {
    case B_ERROR_ABORT:
    case B_ERROR_IGNORE:
        return false;
    case B_ERROR_RETRY:
        goto retry;
    }
#endif
}

static B_FUNC
process_loop_stop_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);

    if (loop->loop_state == B_PROCESS_LOOP_NOT_RUNNING) {
        return true;
    }

    loop->loop_request = B_PROCESS_LOOP_STOP;
    if (!process_loop_interrupt_locked_(loop, eh)) {
        B_ERROR_WHILE_LOCKED();
        return false;
    }

    return true;
}

static B_FUNC
process_loop_wait_for_stop_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);

    while (loop->loop_state != B_PROCESS_LOOP_NOT_RUNNING) {
        int rc = pthread_cond_wait(
            &loop->loop_state_cond,
            &loop->lock);
        if (rc != 0) {
            B_ERROR_WHILE_LOCKED();
            // TODO(strager): Support retry.
            (void) B_RAISE_ERRNO_ERROR(
                eh,
                errno,
                "pthread_cond_wait");
            return false;
        }
    }
    return true;
}

static B_FUNC
set_process_loop_state_locked_(
        struct B_ProcessLoop *loop,
        enum LoopState_ loop_state,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);

    loop->loop_state = loop_state;
retry:;
    int rc = pthread_cond_signal(&loop->loop_state_cond);
    if (rc != 0) {
        B_ERROR_WHILE_LOCKED();
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                rc,
                "pthread_cond_signal")) {
        case B_ERROR_ABORT:
            return false;
        case B_ERROR_IGNORE:
            break;
        case B_ERROR_RETRY:
            goto retry;
        }
    }

    return true;
}

static bool
should_stop_loop_locked_(
        struct B_ProcessLoop *loop) {
    return loop->loop_request == B_PROCESS_LOOP_STOP;
}

static bool
process_is_queued_locked_(
        struct ProcessInfo_ const *proc) {
    B_ASSERT(proc);
    if (proc->pid == B_INVALID_PID) {
        B_ASSERT(proc->args);
        return true;
    } else {
        B_ASSERT(!proc->args);
        return false;
    }
}

static size_t
running_process_count_locked_(
        struct B_ProcessLoop *loop) {
    B_ASSERT(loop);
    size_t count = 0;
    struct ProcessInfo_ *proc;
    LIST_FOREACH(
            proc,
            &loop->processes,
            link) {
        if (!process_is_queued_locked_(proc)) {
            count += 1;
        }
    }
    return count;
}

#if defined(B_CONFIG_KQUEUE)
static B_FUNC
handle_event_locked_(
        struct B_ProcessLoop *loop,
        struct kevent const *event,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    B_ASSERT(event);

    switch (event->filter) {
    case EVFILT_USER:
        // Interrupt event.
        B_ASSERT(event->ident
            == B_PROCESS_LOOP_KQUEUE_USER_IDENT);
        B_LOG(B_DEBUG, "Loop %p interrupted", loop);
        return true;

    case EVFILT_PROC:
        // Process event.
        B_ASSERT(event->fflags
            & (NOTE_EXIT | NOTE_EXITSTATUS));
        int wait_status = (int) event->data;
        struct ProcessInfo_ *proc = event->udata;
        B_ASSERT(proc);
        B_ASSERT(proc->pid == (pid_t) event->ident);
        return process_exited_locked_(
            loop,
            proc,
            wait_status,
            eh);

    default:
        B_BUG();
    }
}
#endif

#if defined(B_CONFIG_EPOLL)
static B_FUNC
handle_event_locked_(
        struct B_ProcessLoop *loop,
        struct epoll_event const *event,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    B_ASSERT(event);

    if (event->data.fd == loop->epoll_interrupt) {
        // Interrupt event.
        B_LOG(B_DEBUG, "Loop %p interrupted", loop);
retry_eventfd_read:;
        eventfd_t value;
        int rc = eventfd_read(loop->epoll_interrupt, &value);
        if (rc == -1) {
            switch (B_RAISE_ERRNO_ERROR(eh, errno, eh)) {
            case B_ERROR_ABORT:
            case B_ERROR_IGNORE:
                return false;
                return false;
            case B_ERROR_RETRY:
                goto retry_eventfd_read;
            }
        }
        return true;
    } else if (event->data.fd == loop->epoll_sigchld) {
        // SIGCHLD.
        bool ok = true;

        // Slurp the signal data.
retry_read:;
        struct signalfd_siginfo siginfo;
        ssize_t rc = read(
            loop->epoll_sigchld,
            &siginfo,
            sizeof(siginfo));
        if (rc == -1) {
            if (errno == EAGAIN) {
                // FIXME(strager): Why do we often get
                // EAGAIN here?
                goto ignore_read;
            }
            switch (B_RAISE_ERRNO_ERROR(eh, errno, eh)) {
            case B_ERROR_ABORT:
                ok = false;
                goto ignore_read;
            case B_ERROR_IGNORE:
                goto ignore_read;
            case B_ERROR_RETRY:
                goto retry_read;
            }
        }
        B_ASSERT(rc == sizeof(siginfo));
        B_ASSERT(siginfo.ssi_signo == SIGCHLD);

ignore_read:
        B_LOG(B_DEBUG, "Loop %p received SIGCHLD", loop);
        ok = check_processes_locked_(loop, eh) && ok;

        return ok;
    } else {
        B_BUG();
    }
}

static B_FUNC
check_processes_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    check_sigprocmask_locked_(loop);
    bool ok = true;
    struct ProcessInfo_ *proc;
    struct ProcessInfo_ *proc_tmp;
    LIST_FOREACH_SAFE(
            proc,
            &loop->processes,
            link,
            proc_tmp) {
        if (process_is_queued_locked_(proc)) {
            continue;
        }
        B_ASSERT(proc->pid != B_INVALID_PID);

retry:;
        int wait_status;
        pid_t rc = waitpid(
            proc->pid,
            &wait_status,
            WNOHANG);
        if (rc == -1) {
            switch (B_RAISE_ERRNO_ERROR(
                    eh,
                    errno,
                    "waitpid")) {
            case B_ERROR_ABORT:
                // Skip this pid.
                ok = false;
                continue;
            case B_ERROR_IGNORE:
                // Skip this pid.
                continue;
            case B_ERROR_RETRY:
                goto retry;
            }
        }
        if (rc == 0) {
            // No status.
            continue;
        }

        B_ASSERT(rc == proc->pid);
        if (WIFSTOPPED(wait_status)
                || WIFCONTINUED(wait_status)) {
            // Ignore.
            continue;
        }

        // NOTE[exit during iteration]: FIXME(strager): What
        // if this causes the next proc to be removed or
        // deallocated?
        ok = process_exited_locked_(
            loop,
            proc,
            wait_status,
            eh) && ok;

        if (should_stop_loop_locked_(loop)) {
            break;
        }
    }
    return ok;
}
#endif

static B_FUNC
process_exited_locked_(
        struct B_ProcessLoop *loop,
        struct ProcessInfo_ *proc,
        int wait_status,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    B_ASSERT(proc);

    int exit_status;
    if (WIFEXITED(wait_status)) {
        exit_status = WEXITSTATUS(wait_status);
        B_LOG(
            B_DEBUG,
            "Loop %p proc %p pid=%d terminated with status %d",
            loop,
            proc,
            proc->pid,
            exit_status);
    } else if (WIFSIGNALED(wait_status)) {
        exit_status = WTERMSIG(wait_status);
        B_ASSERT(exit_status != 0);
        B_LOG(
            B_DEBUG,
            "Loop %p proc %p pid=%d terminated with signal %d",
            loop,
            proc,
            proc->pid,
            exit_status);
    } else if (WIFSTOPPED(wait_status)) {
        B_BUG();
    } else if (WIFCONTINUED(wait_status)) {
        B_BUG();
    } else {
        B_BUG();
    }

    // 1. Remove the process from the loop.  (The kernel
    //    already removed the process from the kqueue.)
    // 2. Call the process' callbacks.
    B_ProcessExitCallback *exit_callback
        = proc->exit_callback;
    void *callback_opaque = proc->callback_opaque;
    LIST_REMOVE(proc, link);
    if (!b_deallocate(proc, eh)) {
        B_ERROR_WHILE_LOCKED();
        // Fall through.
    }

    bool ok;

    B_MUTEX_MUST_UNLOCK(loop->lock, eh);
    {
        ok = exit_callback(
            exit_status,
            callback_opaque,
            eh);
    }
    ok = B_MUTEX_LOCK(loop->lock, eh) && ok;

    return ok;
}

#if defined(B_CONFIG_EPOLL)
static void
check_sigprocmask_locked_(
        struct B_ProcessLoop *loop) {
    if (loop->configuration
            != B_PROCESS_CONFIGURATION_SIGCHLD_MASKED) {
        return;
    }

    sigset_t sigmask;
    int rc = sigprocmask(SIG_BLOCK, NULL, &sigmask);
    B_ASSERT(rc == 0);

    if (!sigismember(&sigmask, SIGCHLD)) {
        fprintf(
            stderr,
            "b: FATAL: SIGCHLD is not masked (see B_PROCESS_CONFIGURATION_SIGCHLD_MASKED)\n");
        fflush(stderr);
        abort();
    }
}
#endif
