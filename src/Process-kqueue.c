// TODO(strager): We should be notified of and handle
// SIGCHLD signals in order to avoid race conditions.

#include <B/Config.h>

#if defined(B_CONFIG_KQUEUE)

# if !defined(B_CONFIG_POSIX_SPAWN)
#  error "posix_spawn support is assumed for the kqueue B_ProcessLoop implementation"
# endif
# if !defined(B_CONFIG_PTHREAD)
#  error "pthread support is assumed for the kqueue B_ProcessLoop implementation"
# endif

# include <B/Alloc.h>
# include <B/Assert.h>
# include <B/Error.h>
# include <B/Log.h>
# include <B/Process.h>

# include <errno.h>
# include <pthread.h>
# include <spawn.h>
# include <sys/event.h>
# include <unistd.h>

// FIXME(strager): Reporting errors while holding a lock is
// a bad idea!
#define B_ERROR_WHILE_LOCKED() B_BUG();

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

    int queue;
    size_t concurrent_process_limit;

    enum LoopState_ loop_state;
    enum LoopRequest_ loop_request;

    // FIXME(strager)
    LIST_HEAD(foo, ProcessInfo_) processes;
};

struct ProcessInfo_ {
    B_ProcessExitCallback *const exit_callback;
    B_ProcessErrorCallback *const error_callback;
    void *const callback_opaque;
    pid_t const pid;

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
process_loop_interrupt_locked_(
        struct B_ProcessLoop *,
        struct B_ErrorHandler const *);

static B_FUNC
handle_event_locked_(
        struct B_ProcessLoop *,
        struct kevent const *,
        struct B_ErrorHandler const *);

static B_FUNC
process_exited_locked_(
        struct B_ProcessLoop *,
        struct ProcessInfo_ *,
        int exit_status,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_process_loop_allocate(
        size_t concurrent_process_limit,
        B_OUTPTR struct B_ProcessLoop **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    struct B_ProcessLoop *loop;
    if (!b_allocate(sizeof(*loop), (void **) &loop, eh)) {
        return false;
    }

retry_kqueue:;
    int queue = kqueue();
    if (queue == -1) {
        switch (B_RAISE_ERRNO_ERROR(eh, errno, "kqueue")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            (void) b_deallocate(loop, eh);
            return false;

        case B_ERROR_RETRY:
            goto retry_kqueue;
        }
    }

    *loop = (struct B_ProcessLoop) {
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .queue = queue,
        .concurrent_process_limit
            = concurrent_process_limit,
        .loop_state = B_PROCESS_LOOP_NOT_RUNNING,
        .loop_request = B_PROCESS_LOOP_CONTINUE,
        .processes
            = LIST_HEAD_INITIALIZER(&loop->processes),
    };

retry_mutex_init:;
    int rc = pthread_mutex_init(&loop->lock, NULL);
    if (rc != 0) {
        switch (B_RAISE_ERRNO_ERROR(
                eh,
                rc,
                "pthread_mutex_init")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            (void) b_deallocate(loop, eh);
            // TODO(strager): Error reporting.
            (void) close(queue);
            return false;

        case B_ERROR_RETRY:
            goto retry_mutex_init;
        }
    }

    B_LOG(B_DEBUG, "Created loop %p", loop);

    *out = loop;
    return true;
}

B_EXPORT_FUNC
b_process_loop_deallocate(
        B_TRANSFER struct B_ProcessLoop *loop,
        uint64_t process_force_kill_timeout_picoseconds,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);

    bool ok = true;

    // TODO(strager): Kill processes and stop loop.
    (void) process_force_kill_timeout_picoseconds;

    // TODO(strager): Error reporting.
    (void) close(loop->queue);
    int rc = pthread_mutex_destroy(&loop->lock);
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

    rc = pthread_mutex_lock(&closure.lock);
    B_ASSERT(rc == 0);
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
    rc = pthread_mutex_unlock(&closure.lock);
    B_ASSERT(rc == 0);

    return ok;
}

B_EXPORT_FUNC
b_process_loop_run_sync(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, loop);

    // TODO(strager): Proper error handling.
    int rc;

    rc = pthread_mutex_lock(&loop->lock);
    if (rc != 0) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            rc,
            "pthread_mutex_lock");
        return false;
    }
    B_ASSERT(rc == 0);

    bool ok = run_sync_locked_(loop, eh);
    if (!ok) B_ERROR_WHILE_LOCKED();

    rc = pthread_mutex_unlock(&loop->lock);
    B_ASSERT(rc == 0);

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
    (void) loop;
    (void) eh;
    B_NYI();
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

    int rc;

    struct ProcessInfo_ *proc;
    if (!b_allocate(sizeof(*proc), (void **) &proc, eh)) {
        return false;
    }

    rc = pthread_mutex_lock(&loop->lock);
    B_ASSERT(rc == 0);  // FIXME(strager)

    if (!process_loop_interrupt_locked_(loop, eh)) {
        B_ERROR_WHILE_LOCKED();
        rc = pthread_mutex_unlock(&loop->lock);
        B_ASSERT(rc == 0);
        (void) b_deallocate(proc, eh);
        return false;
    }

    // NOTE[late spawn]: Spawn as late as possible, so we
    // don't need to kill the process when unwinding due to
    // an error.
    pid_t pid;
    if (!spawn_process_(args, &pid, eh)) {
        B_ERROR_WHILE_LOCKED();
        (void) b_deallocate(proc, eh);
        return false;
    }
    *proc = (struct ProcessInfo_) {
        .exit_callback = exit_callback,
        .error_callback = error_callback,
        .callback_opaque = callback_opaque,
        .pid = pid,
        // .link
    };

    LIST_INSERT_HEAD(&loop->processes, proc, link);

    rc = kevent(loop->queue, &(struct kevent) {
        .ident = pid,
        .filter = EVFILT_PROC,
        .flags = EV_ADD,
        .fflags = NOTE_EXIT | NOTE_EXITSTATUS,
        .data = (intptr_t) NULL,
        .udata = proc,
    }, 1, NULL, 0, NULL);
    B_ASSERT(rc == 0);  // FIXME(strager)

    rc = pthread_mutex_unlock(&loop->lock);
    B_ASSERT(rc == 0);

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

    // See NOTE[loop_run_async synchronization].
    struct ProcessLoopRunAsyncClosure_ *closure = opaque;
    B_ASSERT(closure);

    rc = pthread_mutex_lock(&closure->lock);
    B_ASSERT(rc == 0);

    struct B_ProcessLoop *loop = closure->loop;
    B_ASSERT(loop);
    rc = pthread_mutex_lock(&loop->lock);
    if (rc != 0) {
        closure->errored = true;
        closure->error.errno_value = rc;
        rc = pthread_cond_signal(&closure->cond);
        B_ASSERT(rc == 0);
        rc = pthread_mutex_unlock(&closure->lock);
        B_ASSERT(rc == 0);
        return NULL;
    }

    closure->running = true;
    rc = pthread_cond_signal(&closure->cond);
    B_ASSERT(rc == 0);
    rc = pthread_mutex_unlock(&closure->lock);
    B_ASSERT(rc == 0);

    (void) run_sync_locked_(loop, NULL);

    rc = pthread_mutex_unlock(&loop->lock);
    B_ASSERT(rc == 0);

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
    while (loop->loop_request != B_PROCESS_LOOP_STOP) {
        loop->loop_state = B_PROCESS_LOOP_POLLING;
        int queue = loop->queue;

        size_t events_received;

        bool ok;
        enum B_ErrorHandlerResult error_result;

        // Poll for events, unlocked.  queue should not be
        // closed because loop->loop_state ==
        // B_PROCESS_LOOP_POLLING.
        size_t const events_count = 5;
        struct kevent events[events_count];
        rc = pthread_mutex_unlock(&loop->lock);
        B_ASSERT(rc == 0);
        {  // Unlocked block.
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
        }
        rc = pthread_mutex_lock(&loop->lock);
        B_ASSERT(rc == 0);  // FIXME(strager)

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

        if (loop->loop_request == B_PROCESS_LOOP_STOP) {
            // FIXME(strager): What about events?  Should we
            // re-add them to the kqueue somehow?  Should we
            // process them?
            break;
        }

        // Handle events.
        loop->loop_state = B_PROCESS_LOOP_BUSY;
        for (size_t i = 0; i < events_received; ++i) {
            ok = handle_event_locked_(loop, &events[i], eh)
                && ok;
        }
        if (!ok) {
            goto fail_locked;
        }
    }
    B_ASSERT(loop->loop_request == B_PROCESS_LOOP_STOP);
    loop->loop_state = B_PROCESS_LOOP_NOT_RUNNING;
    loop->loop_request = B_PROCESS_LOOP_CONTINUE;
    B_LOG(B_DEBUG, "Stopped loop %p", loop);
    return true;

fail_locked:
    B_LOG(B_DEBUG, "Stopped loop %p due to failure", loop);
    loop->loop_state = B_PROCESS_LOOP_NOT_RUNNING;
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
process_loop_interrupt_locked_(
        struct B_ProcessLoop *loop,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);

    // No need to interrupt if the process isn't running or
    // should be stopped anyway.
    if (loop->loop_state == B_PROCESS_LOOP_NOT_RUNNING
            || loop->loop_request == B_PROCESS_LOOP_STOP) {
        return true;
    }

    // Signal the running process loop with an EVFILT_USER.
retry:;
    int rc = kevent(loop->queue, &(struct kevent) {
        .ident = B_PROCESS_LOOP_KQUEUE_USER_IDENT,
        .filter = EVFILT_USER,
        .flags = EV_ADD | EV_ONESHOT,
        .fflags = NOTE_FFCOPY | NOTE_TRIGGER,
        .data = (intptr_t) NULL,
        .udata = NULL,
    }, 1, NULL, 0, NULL);
    if (rc == -1) {
        // FIXME(strager): Reporting errors while holding a
        // lock is a bad idea!
        B_BUG();
        switch (B_RAISE_ERRNO_ERROR(eh, errno, "kevent")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        case B_ERROR_RETRY:
            goto retry;
        }
    }
    return true;
}

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
        int exit_status = (int) event->data;
        struct ProcessInfo_ *proc = event->udata;
        B_ASSERT(proc->pid == (pid_t) event->ident);
        B_LOG(
            B_DEBUG,
            "Loop %p proc %p pid=%ld terminated with status %d",
            loop,
            proc,
            (long) proc->pid,
            exit_status);
        B_ASSERT(proc);
        return process_exited_locked_(
                loop,
                proc,
                exit_status,
                eh);

    default:
        B_BUG();
    }
}

static B_FUNC
process_exited_locked_(
        struct B_ProcessLoop *loop,
        struct ProcessInfo_ *proc,
        int exit_status,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(loop);
    B_ASSERT(proc);

    int rc;

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

    rc = pthread_mutex_unlock(&loop->lock);
    B_ASSERT(rc == 0);
    {
        ok = exit_callback(
            exit_status,
            callback_opaque,
            eh);
    }
    rc = pthread_mutex_lock(&loop->lock);
    B_ASSERT(rc == 0);

    return ok;
}

#endif
