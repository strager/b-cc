#define B_DEBUG
#define B_VALGRIND

#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Portable.h>
#include <B/Internal/PortableUContext.h>
#include <B/Log.h>

#include <zmq.h>

#if defined(B_VALGRIND)
#include <valgrind/valgrind.h>
#endif

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>

#if defined(B_DEBUG)
// TODO(strager): Make portable.
# if defined(__APPLE__)

#  include <libkern/OSAtomic.h>
#  include <pthread.h>
#  include <stdbool.h>

// Contains a thread-specific pointer to the
// last-{switched,allocated} FiberContext, or NULL if no
// allocation ever occured or if the FiberContext was
// deallocated.
static pthread_key_t
current_fiber_context_key;
static bool
current_fiber_context_key_initd = false;
static OSSpinLock
current_fiber_context_key_spinlock = OS_SPINLOCK_INIT;

static uintptr_t
max_fiber_id = ((uintptr_t) NULL) + 1;
static OSSpinLock
max_fiber_id_spinlock = OS_SPINLOCK_INIT;
# else
#  error Unsupported platform
# endif
#endif

enum B_FiberContextYieldType {
    // Polls with timeout=0.  Resumes a fiber (except the
    // current fiber).  (The current fiber must not be
    // polling.)
    B_FIBER_CONTEXT_YIELD_HARD,

    // Resumes a non-polling fiber (including the current
    // fiber).  Polls with timeout=N otherwise.
    B_FIBER_CONTEXT_YIELD_SOFT,
};

enum B_FiberContextPollType {
    B_FIBER_CONTEXT_POLL_BLOCKING,
    B_FIBER_CONTEXT_POLL_NONBLOCKING,
};

struct B_FiberPoll {
    // Inputs.
    int volatile pollitem_count;
    long volatile timeout_milliseconds;

    // Inputs/outputs.
    zmq_pollitem_t *volatile pollitems;

    // Outputs.
    int volatile ready_pollitems;
    // FIXME(strager): Shared ownership???
    struct B_Exception *volatile ex;
};

struct B_Fiber {
    struct B_UContext context;

#if defined(B_VALGRIND)
    unsigned int valgrind_stack_id;
#endif

    // If NULL, the fiber is not polling or has gotten poll
    // results already.
    struct B_FiberPoll *volatile poll;

    // When picking a non-polling fiber to awaken, we pick
    // the least recent fiber (i.e. the fiber with the
    // minimum 'recency').
    uint32_t recency;

#if defined(B_DEBUG)
    uintptr_t fiber_id;
#endif
};

struct B_FiberContext {
    // Vector of suspended fibers.
    size_t volatile fibers_size;  // Allocated.
    size_t volatile fiber_count;  // In use.
    struct B_Fiber *volatile fibers;  // Unboxed vector.

    bool volatile is_finishing;

    // Maximum of struct B_Fiber::recency in 'fibers'.
    uint32_t max_recency;

#if defined(B_DEBUG)
    uintptr_t current_fiber_id;
#endif
};

static struct B_Fiber *
b_fiber_context_get_non_polling_fiber(
    struct B_FiberContext *);

static B_ERRFUNC
b_fiber_context_get_free_fiber(
    struct B_FiberContext *,
    struct B_Fiber **out);

static B_ERRFUNC
b_fiber_context_allocate_recency(
    struct B_FiberContext *,
    uint32_t *out_recency);

#if defined(B_DEBUG)
static B_ERRFUNC
b_fiber_context_allocate_fiber_id(
    uintptr_t *out_fiber_id);

static B_ERRFUNC
b_fiber_context_set_current(
    struct B_FiberContext *);
#endif

static void
b_fibers_poll(
    struct B_FiberContext *,
    enum B_FiberContextPollType);

static void
b_fiber_context_switch(
    struct B_FiberContext *,
    struct B_Fiber *,
    struct B_FiberPoll *);

static B_ERRFUNC
b_fiber_context_yield(
    struct B_FiberContext *,
    struct B_FiberPoll *,
    enum B_FiberContextYieldType);

static void B_NO_RETURN
b_fiber_yield_end(
    struct B_FiberContext *);

static void
clear_revents(
    zmq_pollitem_t *pollitems,
    size_t pollitem_count);

B_ERRFUNC
b_fiber_context_allocate(
    struct B_FiberContext **out) {

#if defined(B_DEBUG)
    uintptr_t fiber_id;
    struct B_Exception *ex
        = b_fiber_context_allocate_fiber_id(
            &fiber_id);
    if (ex) {
        return ex;
    }
#endif

    B_ALLOCATE(struct B_FiberContext, fiber_context, {
        .fibers_size = 0,
        .fiber_count = 0,
        .fibers = NULL,

        .is_finishing = false,

        .max_recency = 0,

#if defined(B_DEBUG)
        .current_fiber_id = fiber_id,
#endif
    });

#if defined(B_DEBUG)
    (void) b_fiber_context_set_current(fiber_context);
#endif

    *out = fiber_context;
    return NULL;
}

B_ERRFUNC
b_fiber_context_deallocate(
    struct B_FiberContext *fiber_context) {

#if defined(B_DEBUG)
    (void) b_fiber_context_set_current(NULL);
#endif

    // TODO(strager): Ensure no fibers are alive (except the
    // current one, of course).
    free(fiber_context->fibers);
    free(fiber_context);

    return NULL;
}

B_ERRFUNC
b_fiber_context_finish(
    struct B_FiberContext *fiber_context) {

    if (fiber_context->is_finishing) {
        return b_exception_string(
            "Already in the process of finishing");
    }

    fiber_context->is_finishing = true;
    {
        {
            struct B_Exception *ex
                = b_fiber_context_yield(
                    fiber_context,
                    NULL,
                    B_FIBER_CONTEXT_YIELD_HARD);
            if (ex) {
                return ex;
            }
        }
        assert(fiber_context->fiber_count == 0);
    }
    fiber_context->is_finishing = false;

    return NULL;
}

B_ERRFUNC
b_fiber_context_poll_zmq(
    struct B_FiberContext *fiber_context,
    zmq_pollitem_t *pollitems,
    int pollitem_count,
    long timeout_milliseconds,
    int *ready_pollitems,
    bool *is_finished) {

    // TODO(strager): Support timeout.
    assert(timeout_milliseconds == -1);

    clear_revents(pollitems, pollitem_count);
    if (ready_pollitems) {
        *ready_pollitems = 0;
    }

    if (fiber_context->is_finishing) {
        *is_finished = true;
        return NULL;
    }

    struct B_FiberPoll fiber_poll = {
        .pollitem_count = pollitem_count,
        .timeout_milliseconds = timeout_milliseconds,

        .pollitems = pollitems,

        .ready_pollitems = 0,
        .ex = NULL,
    };

    struct B_Exception *ex
        = b_fiber_context_yield(
            fiber_context,
            &fiber_poll,
            B_FIBER_CONTEXT_YIELD_SOFT);
    if (ex) {
        return ex;
    }

    if (fiber_context->is_finishing) {
        *is_finished = true;
        return NULL;
    }

    if (ready_pollitems) {
        *ready_pollitems = fiber_poll.ready_pollitems;
    }

    if (is_finished) {
        *is_finished = false;
    }

    return fiber_poll.ex;
}

struct B_FiberTrampolineClosure {
    struct B_FiberContext *fiber_context;
    void *(*callback)(void *user_closure);
    void *user_closure;
    void **out_callback_result;

    struct B_UContext const *parent_context;
    struct B_UContext *child_context;
};

static void B_NO_RETURN
b_fiber_trampoline(
    void *closure_raw) {

    struct B_FiberTrampolineClosure closure
        = *(struct B_FiberTrampolineClosure *) closure_raw;

    // See corresponding b_ucontext_swapcontext in
    // b_fiber_context_fork.
    b_ucontext_swapcontext(
        closure.child_context,
        closure.parent_context);
    // closure_raw is now invalid.

    B_LOG_FIBER(
        B_FIBER,
        closure.fiber_context,
        "Fiber started");
    void *result = closure.callback(closure.user_closure);
    if (closure.out_callback_result) {
        *closure.out_callback_result = result;
    }

    B_LOG_FIBER(
        B_FIBER,
        closure.fiber_context,
        "Fiber dying");

    b_fiber_yield_end(closure.fiber_context);
}

B_ERRFUNC
b_fiber_context_fork(
    struct B_FiberContext *fiber_context,
    void *(*callback)(void *user_closure),
    void *user_closure,
    void **out_callback_result) {

    struct B_Exception *ex;

    struct B_Fiber *fiber;
    ex = b_fiber_context_get_free_fiber(
        fiber_context,
        &fiber);
    if (ex) {
        return ex;
    }

    ex = b_fiber_context_allocate_recency(
        fiber_context,
        &fiber->recency);
    if (ex) {
        return ex;
    }
#if defined(B_DEBUG)
    ex = b_fiber_context_allocate_fiber_id(
        &fiber->fiber_id);
    if (ex) {
        return ex;
    }
#endif
    fiber->poll = NULL;

    struct B_UContext *context = &fiber->context;

    // TODO(strager): Factor out stack allocation.
    // TODO(strager): Free the stacks!
    size_t stack_size = SIGSTKSZ;
    void *stack = malloc(stack_size);
#if defined(B_VALGRIND)
    fiber->valgrind_stack_id = VALGRIND_STACK_REGISTER(
        stack,
        stack + stack_size);
#endif

    struct B_UContext self_context;
    struct B_FiberTrampolineClosure trampoline_closure = {
        .fiber_context = fiber_context,
        .callback = callback,
        .user_closure = user_closure,
        .out_callback_result = out_callback_result,

        .parent_context = &self_context,
        .child_context = context,
    };

    b_ucontext_makecontext(
        context,
        stack,
        stack_size,
        b_fiber_trampoline,
        &trampoline_closure);

    // Trampoline should call us back ASAP.
    b_ucontext_swapcontext(&self_context, context);

#if defined(B_DEBUG)
    // NOTE(strager): Because we didn't do a formal fiber
    // swap, the 'fiber' pointer should still be valid.
    B_LOG_FIBER(
        B_INFO,
        fiber_context,
        "Forked fiber %p.",
        (void *) fiber->fiber_id);
#endif

    return NULL;
}

B_ERRFUNC
b_fiber_context_soft_yield(
    struct B_FiberContext *fiber_context) {

    struct B_Exception *ex = b_fiber_context_yield(
        fiber_context,
        NULL,
        B_FIBER_CONTEXT_YIELD_SOFT);
    if (ex) {
        return ex;
    }

    return NULL;
}

B_ERRFUNC
b_fiber_context_hard_yield(
    struct B_FiberContext *fiber_context) {

    struct B_Exception *ex = b_fiber_context_yield(
        fiber_context,
        NULL,
        B_FIBER_CONTEXT_YIELD_HARD);
    if (ex) {
        return ex;
    }

    return NULL;
}

B_ERRFUNC
b_fiber_context_current_fiber_id(
    struct B_FiberContext *fiber_context,
    void **out_fiber_id) {

#if defined(B_DEBUG)
    if (!fiber_context) {
#if defined(__APPLE__)
        OSSpinLockLock(&current_fiber_context_key_spinlock);

        if (current_fiber_context_key_initd) {
            fiber_context = pthread_getspecific(
                current_fiber_context_key);
        }

        OSSpinLockUnlock(
            &current_fiber_context_key_spinlock);
#else
#error Unsupported platform
#endif
    }

    if (!fiber_context) {
        return b_exception_string(
            "No fiber context available");
    }

    *out_fiber_id
        = (void *) fiber_context->current_fiber_id;
    return NULL;
#else
    (void) fiber_context;
    (void) out_fiber_id;
    return b_exception_string(
        "Only supported in debug builds");
#endif
}

static B_ERRFUNC
b_fiber_context_get_free_fiber(
    struct B_FiberContext *fiber_context,
    struct B_Fiber **out) {

    // Grow vector if necessary.
    if (fiber_context->fiber_count
        >= fiber_context->fibers_size) {

        size_t old_size = fiber_context->fibers_size;
        struct B_Fiber *old_fibers = fiber_context->fibers;

        size_t new_size = old_size ? old_size * 2 : 2;
        assert(new_size > old_size);

        struct B_Fiber *new_fibers = malloc(
            sizeof(struct B_Fiber) * new_size);
        if (!new_fibers) {
            return b_exception_memory();
        }

        for (size_t i = 0; i < old_size; ++i) {
            b_ucontext_copy(
                &new_fibers[i].context,
                &old_fibers[i].context);
            new_fibers[i].poll = old_fibers[i].poll;
            new_fibers[i].recency = old_fibers[i].recency;
#if defined(B_DEBUG)
            new_fibers[i].fiber_id = old_fibers[i].fiber_id;
#endif
        }
        free(old_fibers);

        fiber_context->fibers_size = new_size;
        fiber_context->fibers = new_fibers;
    }

    assert(fiber_context->fiber_count + 1
        <= fiber_context->fibers_size);
    *out = &fiber_context
        ->fibers[fiber_context->fiber_count];
    fiber_context->fiber_count += 1;
    return NULL;
}

static B_ERRFUNC
b_fiber_context_allocate_recency(
    struct B_FiberContext *fiber_context,
    uint32_t *out_recency) {

    // TODO(strager): Overflow checking.

    uint32_t recency = fiber_context->max_recency;
    fiber_context->max_recency += 1;

    *out_recency = recency;
    return NULL;
}

#if defined(B_DEBUG)
static B_ERRFUNC
b_fiber_context_allocate_fiber_id(
    uintptr_t *out_fiber_id) {

#if defined(__APPLE__)
    OSSpinLockLock(&max_fiber_id_spinlock);
#else
#error Unsupported platform
#endif

    max_fiber_id += 1;
    uintptr_t fiber_id = max_fiber_id;

#if defined(__APPLE__)
    OSSpinLockUnlock(&max_fiber_id_spinlock);
#else
#error Unsupported platform
#endif

    *out_fiber_id = fiber_id;
    return NULL;
}

static B_ERRFUNC
b_fiber_context_set_current(
    struct B_FiberContext *fiber_context) {

#if defined(__APPLE__)
    struct B_Exception *ex = NULL;
    int rc;

    OSSpinLockLock(&current_fiber_context_key_spinlock);

    rc = pthread_key_create(
        &current_fiber_context_key, NULL);
    if (rc != 0) {
        ex = b_exception_errno("pthread_key_create", rc);
    }

    if (!ex) {
        current_fiber_context_key_initd = true;
    }

    OSSpinLockUnlock(&current_fiber_context_key_spinlock);

    if (ex) {
        return ex;
    }

    rc = pthread_setspecific(
        current_fiber_context_key,
        fiber_context);
    if (rc != 0) {
        return b_exception_errno("pthread_setspecific", rc);
    }

    return NULL;
#else
#error Unsupported platform
#endif
}
#endif

// Poll results (and errors) are stored in the fibers.
static void
b_fibers_poll(
    struct B_FiberContext *fiber_context,
    enum B_FiberContextPollType poll_type) {

    struct B_Fiber *fibers = fiber_context->fibers;
    size_t fiber_count = fiber_context->fiber_count;

    // Count poll items.
    size_t pollitem_total_count = 0;
    for (size_t i = 0; i < fiber_count; ++i) {
        struct B_Fiber const *fiber = &fibers[i];
        if (fiber->poll) {
            pollitem_total_count
                += fiber->poll->pollitem_count;
        }
    }

    // Put poll items into a single array.
    // TODO(strager): Deduplicate file descriptors.
    zmq_pollitem_t pollitems[pollitem_total_count];
    {
        zmq_pollitem_t *cur_pollitem = pollitems;
        for (size_t i = 0; i < fiber_count; ++i) {
            struct B_Fiber const *fiber = &fibers[i];
            if (!fiber->poll) {
                continue;
            }

            assert(fiber->poll->pollitems);
            memcpy(
                cur_pollitem,
                fiber->poll->pollitems,
                fiber->poll->pollitem_count
                    * sizeof(zmq_pollitem_t));
            cur_pollitem += fiber->poll->pollitem_count;
        }

        assert(cur_pollitem
            == pollitems + pollitem_total_count);
    }

    // TODO(strager): Support timeouts.
    for (size_t i = 0; i < fiber_count; ++i) {
        if (fibers[i].poll) {
            assert(fibers[i].poll->timeout_milliseconds
                < 0);
        }
    }

    // Poll!
    B_LOG_FIBER(
        B_FIBER,
        fiber_context,
        "Polling %s %zu pollitems across %zu fibers",
        poll_type == B_FIBER_CONTEXT_POLL_BLOCKING
            ? "blocking" : "non-blocking",
        pollitem_total_count,
        fiber_count);
    for (size_t i = 0; i < fiber_count; ++i) {
        struct B_Fiber const *fiber = &fibers[i];
        if (!fiber->poll) {
            continue;
        }

        for (size_t j = 0; j < fiber->poll->pollitem_count; ++j) {
            B_LOG_FIBER(
                B_FIBER,
                fiber_context,
                "fiber[%zu]"
#if defined(B_DEBUG)
                "=%p"
#endif
                " pollitem[%zu] socket=%p events=%x",
                i,
#if defined(B_DEBUG)
                (void *) fiber->fiber_id,
#endif
                j,
                fiber->poll->pollitems[j].socket,
                fiber->poll->pollitems[j].events);
        }
    }

    long timeout_milliseconds;
    switch (poll_type) {
    case B_FIBER_CONTEXT_POLL_BLOCKING:
        timeout_milliseconds = -1;
        break;
    case B_FIBER_CONTEXT_POLL_NONBLOCKING:
        timeout_milliseconds = 0;
        break;
    }

    int rc = zmq_poll(
        pollitems,
        pollitem_total_count,
        timeout_milliseconds);

    struct B_Exception *poll_ex = NULL;
    int ready_pollitems = 0;
    if (rc == -1) {
        poll_ex = b_exception_errno("zmq_poll", errno);
    } else {
        ready_pollitems = rc;
    }

    if (!poll_ex) {
        B_LOG_FIBER(
            B_FIBER,
            fiber_context,
            "Polling finished with %d ready pollitems:",
            ready_pollitems);
        zmq_pollitem_t const *cur_pollitem = pollitems;
        for (size_t i = 0; i < fiber_count; ++i) {
            struct B_Fiber const *fiber = &fibers[i];
            if (!fiber->poll) {
                continue;
            }

            for (size_t j = 0; j < fiber->poll->pollitem_count; ++j) {
                B_LOG_FIBER(
                    B_FIBER,
                    fiber_context,
                    "fiber[%zu] pollitem[%zu] socket=%p revents=%x",
                    i,
                    j,
                    cur_pollitem->socket,
                    cur_pollitem->revents);
                ++cur_pollitem;
            }
        }
    }

    bool const waken_all_fibers
        = poll_ex || ready_pollitems == 0;

    // Copy results of polling to fibers.
    {
        int counted_ready_pollitems = 0;
        zmq_pollitem_t const *cur_pollitem = pollitems;
        for (
            size_t fiber_index = 0;
            fiber_index < fiber_count;
            ++fiber_index) {

            struct B_Fiber *fiber = &fibers[fiber_index];
            if (!fiber->poll) {
                continue;
            }

            assert(fiber->poll->pollitems);

            zmq_pollitem_t *pollitems
                = fiber->poll->pollitems;
            size_t pollitem_count
                = fiber->poll->pollitem_count;

            // Copy revents and count ready pollitems.
            int cur_ready_pollitems = 0;
            for (size_t i = 0; i < pollitem_count; ++i) {
                short revents = cur_pollitem->revents;
                if (revents) {
                    cur_ready_pollitems += 1;
                }
                pollitems[i].revents = revents;
                ++cur_pollitem;
            }
            counted_ready_pollitems += cur_ready_pollitems;

            fiber->poll->ready_pollitems
                = cur_ready_pollitems;
            fiber->poll->ex = poll_ex;

            // If polling completes, we should allow the
            // fiber to be scheduled again.  We need to wake
            // up all fibers in case of an error or timeout,
            // else we will probably deadlock.
            if (cur_ready_pollitems || waken_all_fibers) {
                fiber->poll = NULL;
            }
        }

        assert(cur_pollitem
            == pollitems + pollitem_total_count);
        assert(counted_ready_pollitems == ready_pollitems);
    }

    // Make sure we woke at least one fiber.
    size_t woken_fibers = 0;
    for (size_t i = 0; i < fiber_count; ++i) {
        if (!fibers[i].poll) {
            woken_fibers += 1;
        }
    }
    assert(woken_fibers > 0);
}

// Switch to another fiber, putting the current context
// (fiber) in its place.
static void
b_fiber_context_switch(
    struct B_FiberContext *fiber_context,
    struct B_Fiber *fiber,
    struct B_FiberPoll *poll) {

    assert(!fiber->poll);

    uint32_t recency;
    struct B_Exception *ex
        = b_fiber_context_allocate_recency(
            fiber_context,
            &recency);
    if (ex) {
        B_LOG_EXCEPTION(ex);
        abort();
    }

#if defined(B_DEBUG)
    uintptr_t fiber_id = fiber->fiber_id;
    fiber->fiber_id = fiber_context->current_fiber_id;
    fiber_context->current_fiber_id = fiber_id;
#endif

    struct B_UContext other_fiber_context;
    b_ucontext_copy(
        &other_fiber_context,
        &fiber->context);
    fiber->poll = poll;
    fiber->recency = recency;

#if defined(B_DEBUG)
    (void) b_fiber_context_set_current(fiber_context);
#endif

    // NOTE(strager): fiber (the pointer) is invalid after
    // calling swapcontext.
    b_ucontext_swapcontext(
        &fiber->context,
        &other_fiber_context);
}

static B_ERRFUNC
b_fiber_context_yield(
    struct B_FiberContext *fiber_context,
    struct B_FiberPoll *poll,
    enum B_FiberContextYieldType yield_type) {

    if (yield_type == B_FIBER_CONTEXT_YIELD_HARD) {
        assert(!poll);

        if (fiber_context->fiber_count == 0) {
            // FIXME(strager): Should this be an error?
            return NULL;
        }

        b_fibers_poll(
            fiber_context,
            B_FIBER_CONTEXT_POLL_NONBLOCKING);
    }

    // Switch to an awake fiber.
    {
        struct B_Fiber *non_polling_fiber
            = b_fiber_context_get_non_polling_fiber(
                fiber_context);
        if (non_polling_fiber) {
            b_fiber_context_switch(
                fiber_context,
                non_polling_fiber,
                poll);
            return NULL;
        }

        // FIXME(strager): Refactor hard yielding.
        if (yield_type == B_FIBER_CONTEXT_YIELD_HARD) {
            abort();  // FIXME(strager)
        }
    }

    if (yield_type == B_FIBER_CONTEXT_YIELD_SOFT && !poll) {
        // All fibers are polling, but we are not, so we can
        // resume this fiber.
        return NULL;
    }

    // Add this fiber to the poll list.
    struct B_Fiber *fiber;
    if (poll) {
        {
            struct B_Exception *ex
                = b_fiber_context_get_free_fiber(
                    fiber_context,
                    &fiber);
            if (ex) {
                return ex;
            }
        }

        {
            struct B_Exception *ex
                = b_fiber_context_allocate_recency(
                    fiber_context,
                    &fiber->recency);
            if (ex) {
                return ex;
            }
        }
        fiber->poll = poll;
        // NOTE(strager): struct B_Fiber::context is not
        // used by b_fibers_poll.
    }

    b_fibers_poll(
        fiber_context,
        B_FIBER_CONTEXT_POLL_BLOCKING);

    // Remove the current fiber.
    if (poll) {
        assert(fiber == &fiber_context
            ->fibers[fiber_context->fiber_count - 1]);
        fiber_context->fiber_count -= 1;
    }

    // Switch to a woken fiber.
    {
        struct B_Fiber *non_polling_fiber
            = b_fiber_context_get_non_polling_fiber(
                fiber_context);
        if (non_polling_fiber) {
            b_fiber_context_switch(
                fiber_context,
                non_polling_fiber,
                poll);
            return NULL;
        } else if (poll) {
            // The current fiber was woken.
            return NULL;
        } else {
            // FIXME(strager): When does this occur?  When
            // this is the only fiber?
            abort();
        }
    }
}

static void B_NO_RETURN
b_fiber_yield_end(
    struct B_FiberContext *fiber_context) {

    // TODO(strager): Merge with b_fiber_context_yield.

    assert(fiber_context->fiber_count > 0);

    struct B_Fiber *non_polling_fiber
        = b_fiber_context_get_non_polling_fiber(
            fiber_context);
    if (!non_polling_fiber) {
        b_fibers_poll(
            fiber_context,
            B_FIBER_CONTEXT_POLL_BLOCKING);

        non_polling_fiber
            = b_fiber_context_get_non_polling_fiber(
                fiber_context);
        assert(non_polling_fiber);
    }

    // Pull the context out of the list by moving the last
    // fiber into non_polling_fiber.
    struct B_UContext context;
    b_ucontext_copy(
        &context,
        &non_polling_fiber->context);

    struct B_Fiber *last_fiber = &fiber_context
        ->fibers[fiber_context->fiber_count - 1];
    if (non_polling_fiber != last_fiber) {
        memcpy(
            non_polling_fiber,
            last_fiber,
            sizeof(struct B_Fiber));
    }

    fiber_context->fiber_count -= 1;

    B_LOG_FIBER(
        B_FIBER,
        fiber_context,
        "Fiber dead");
    b_ucontext_setcontext(&context);
}

// Find the non-polling fiber with the minimum recency.
// Returns NULL if all fibers are polling.
static struct B_Fiber *
b_fiber_context_get_non_polling_fiber(
    struct B_FiberContext *fiber_context) {

    struct B_Fiber *min_fiber = NULL;

    size_t fiber_count = fiber_context->fiber_count;
    struct B_Fiber *fibers = fiber_context->fibers;
    for (size_t i = 0; i < fiber_count; ++i) {
        struct B_Fiber *fiber = &fibers[i];
        if (!fiber->poll) {
            bool is_least_recent
                = !min_fiber || (min_fiber
                    && fiber->recency < min_fiber->recency);
            if (is_least_recent) {
                min_fiber = fiber;
            }
        }
    }
    return min_fiber;
}

static void
clear_revents(
    zmq_pollitem_t *pollitems,
    size_t pollitem_count) {

    for (size_t i = 0; i < pollitem_count; ++i) {
        pollitems[i].revents = 0;
    }
}
