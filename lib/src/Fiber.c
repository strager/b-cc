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

enum B_FiberContextYieldType {
    // 1. Polls with timeout=0, ignoring non-polling fibers.
    // 2. Inspects non-polling fibers (except self).
    // 3. Polls with timeout=N, ignoring non-polling fibers.
    B_FIBER_CONTEXT_YIELD_HARD,

    // 1. Inspects non-polling fibers (including self).
    // 2. Polls with timeout=N.  (There should be no
    //    non-polling fibers.)
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
};

struct B_FiberContext {
    // Vector of suspended fibers.
    size_t volatile fibers_size;  // Allocated.
    size_t volatile fiber_count;  // In use.
    struct B_Fiber *volatile fibers;  // Unboxed vector.

    bool volatile is_finishing;
};

static struct B_Fiber *
b_fiber_context_get_non_polling_fiber(
    struct B_FiberContext *);

static B_ERRFUNC
b_fiber_context_get_free_fiber(
    struct B_FiberContext *,
    struct B_Fiber **out);

static void
b_fibers_poll(
    struct B_Fiber *fibers,
    size_t fiber_count,
    enum B_FiberContextPollType);

static void
b_fiber_context_switch(
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

    B_ALLOCATE(struct B_FiberContext, fiber_context, {
        .fibers_size = 0,
        .fiber_count = 0,
        .fibers = NULL,

        .is_finishing = false,
    });
    *out = fiber_context;

    return NULL;
}

B_ERRFUNC
b_fiber_context_deallocate(
    struct B_FiberContext *fiber_context) {

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

    *is_finished = false;
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

    printf("closure_raw=%p\n", closure_raw);
    struct B_FiberTrampolineClosure closure
        = *(struct B_FiberTrampolineClosure *) closure_raw;

    // See corresponding b_ucontext_swapcontext in
    // b_fiber_context_fork.
    b_ucontext_swapcontext(
        closure.child_context,
        closure.parent_context);
    // closure_raw is now invalid.

    printf("callback=%p\n", closure.callback);
    B_LOG(B_FIBER, "Fiber started");
    void *result = closure.callback(closure.user_closure);
    if (closure.out_callback_result) {
        *closure.out_callback_result = result;
    }

    B_LOG(B_FIBER, "Fiber dying");

    b_fiber_yield_end(closure.fiber_context);
}

B_ERRFUNC
b_fiber_context_fork(
    struct B_FiberContext *fiber_context,
    void *(*callback)(void *user_closure),
    void *user_closure,
    void **out_callback_result) {

    struct B_Fiber *fiber;
    struct B_Exception *ex
        = b_fiber_context_get_free_fiber(
            fiber_context,
            &fiber);
    if (ex) {
        return ex;
    }
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

// Poll results (and errors) are stored in the fibers.
static void
b_fibers_poll(
    struct B_Fiber *fibers,
    size_t fiber_count,
    enum B_FiberContextPollType poll_type) {

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
    B_LOG(
        B_FIBER,
        "Polling %zu pollitems across %zu fibers",
        pollitem_total_count,
        fiber_count);
    for (size_t i = 0; i < fiber_count; ++i) {
        struct B_Fiber const *fiber = &fibers[i];
        if (!fiber->poll) {
            continue;
        }

        for (size_t j = 0; j < fiber->poll->pollitem_count; ++j) {
            B_LOG(
                B_FIBER,
                "fiber[%zu] pollitem[%zu] socket=%p events=%x",
                i,
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

    bool const waken_all_fibers
        = poll_ex || (
            ready_pollitems == 0
            && poll_type == B_FIBER_CONTEXT_POLL_BLOCKING);

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
    if (poll_type == B_FIBER_CONTEXT_POLL_BLOCKING) {
        size_t woken_fibers = 0;
        for (size_t i = 0; i < fiber_count; ++i) {
            if (!fibers[i].poll) {
                woken_fibers += 1;
            }
        }
        assert(woken_fibers > 0);
    }
}

// Switch to another fiber, putting the current context
// (fiber) in its place.
static void
b_fiber_context_switch(
    struct B_Fiber *fiber,
    struct B_FiberPoll *poll) {

    assert(!fiber->poll);

    struct B_UContext other_fiber_context;
    b_ucontext_copy(
        &other_fiber_context,
        &fiber->context);

    fiber->poll = poll;
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
        b_fibers_poll(
            fiber_context->fibers,
            fiber_context->fiber_count,
            B_FIBER_CONTEXT_POLL_NONBLOCKING);
    }

    // Switch to an awake fiber.
    {
        struct B_Fiber *non_polling_fiber
            = b_fiber_context_get_non_polling_fiber(
                fiber_context);
        if (non_polling_fiber) {
            b_fiber_context_switch(non_polling_fiber, poll);
            return NULL;
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
        fiber->poll = poll;
        // NOTE(strager): struct B_Fiber::context is not
        // used by b_fibers_poll.
    }

    b_fibers_poll(
        fiber_context->fibers,
        fiber_context->fiber_count,
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
            b_fiber_context_switch(non_polling_fiber, poll);
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

    assert(fiber_context->fiber_count > 0);

    struct B_Fiber *non_polling_fiber
        = b_fiber_context_get_non_polling_fiber(
            fiber_context);
    if (!non_polling_fiber) {
        b_fibers_poll(
            fiber_context->fibers,
            fiber_context->fiber_count,
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

    B_LOG(B_FIBER, "Fiber dead");
    b_ucontext_setcontext(&context);
}

static struct B_Fiber *
b_fiber_context_get_non_polling_fiber(
    struct B_FiberContext *fiber_context) {

    size_t fiber_count = fiber_context->fiber_count;
    struct B_Fiber *fibers = fiber_context->fibers;
    for (size_t i = 0; i < fiber_count; ++i) {
        struct B_Fiber *fiber = &fibers[i];
        if (!fiber->poll) {
            return fiber;
        }
    }
    return NULL;
}

static void
clear_revents(
    zmq_pollitem_t *pollitems,
    size_t pollitem_count) {

    for (size_t i = 0; i < pollitem_count; ++i) {
        pollitems[i].revents = 0;
    }
}
