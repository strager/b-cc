#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/Allocate.h>

#include <zmq.h>

#include <assert.h>
#include <stddef.h>
#include <string.h>

#if defined(__APPLE__)
// These functions are available, but deprecated, in Apple's
// libSystem implementation.
int getcontext(ucontext_t *);
void makecontext(ucontext_t *, void (*)(/* not void! */), int, ...);
int setcontext(ucontext_t const *);
int swapcontext(ucontext_t *, ucontext_t const *);
#else
# include <ucontext.h>
#endif

struct B_FiberPoll {
    // Inputs.
    int pollitem_count;
    long timeout_milliseconds;

    // Inputs/outputs.
    zmq_pollitem_t *pollitems;

    // Outputs.
    int ready_pollitems;
    // FIXME(strager): Shared ownership???
    struct B_Exception *ex;
};

struct B_Fiber {
    ucontext_t context;

    // If NULL, the fiber is not polling or has gotten poll
    // results already.
    struct B_FiberPoll *poll;
};

struct B_FiberContext {
    // Vector of suspended fibers.
    size_t fibers_size;  // Allocated.
    size_t fiber_count;  // In use.
    struct B_Fiber *fibers;  // Unboxed vector.
};

static struct B_Fiber *
b_fiber_context_get_non_polling_fiber(
    struct B_FiberContext *);

static B_ERRFUNC
b_fiber_context_get_free_fiber(
    struct B_FiberContext *,
    struct B_Fiber **out);

static B_ERRFUNC
b_fibers_poll(
    struct B_Fiber *fibers,
    size_t fiber_count);

static void
b_fiber_context_switch(
    struct B_Fiber *,
    struct B_FiberPoll *);

static B_ERRFUNC
b_fiber_context_yield(
    struct B_FiberContext *,
    struct B_FiberPoll *);

B_ERRFUNC
b_fiber_context_allocate(
    struct B_FiberContext **out) {

    B_ALLOCATE(struct B_FiberContext, fiber_context, {
    });
    *out = fiber_context;

    return NULL;
}

B_ERRFUNC
b_fiber_context_deallocate(
    struct B_FiberContext *fiber_context) {

    free(fiber_context);

    return NULL;
}

B_ERRFUNC
b_fiber_context_finish(
    struct B_FiberContext *fiber_context) {

    // TODO(strager)
    (void) fiber_context;
    return NULL;
}

B_ERRFUNC
b_fiber_context_poll_zmq(
    struct B_FiberContext *fiber_context,
    zmq_pollitem_t pollitems[],
    int pollitem_count,
    long timeout_milliseconds,
    int *ready_pollitems) {

    (void) fiber_context;

    (void) b_fiber_context_yield; // TODO(strager)

    int rc = zmq_poll(
        pollitems,
        pollitem_count,
        timeout_milliseconds);
    if (rc == -1) {
        return b_exception_errno("zmq_poll", errno);
    }

    if (ready_pollitems) {
        *ready_pollitems = rc;
    }

    return NULL;
}

B_ERRFUNC
b_fiber_context_fork(
    struct B_FiberContext *fiber_context,
    void *(*callback)(void *user_closure),
    void *user_closure,
    void **out_callback_result) {

    // TODO(strager)

    (void) fiber_context;

    void *callback_result = callback(user_closure);
    if (out_callback_result) {
        *out_callback_result = callback_result;
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

        size_t new_size = fiber_context->fibers_size * 2;
        assert(new_size > fiber_context->fibers_size);
        struct B_Fiber *new_fibers = realloc(
            fiber_context->fibers,
            sizeof(struct B_Fiber) * new_size);
        if (!new_fibers) {
            return b_exception_memory();
        }

        fiber_context->fibers_size = new_size;
        fiber_context->fibers = new_fibers;
    }

    assert(fiber_context->fiber_count + 1
        > fiber_context->fibers_size);
    *out = &fiber_context
        ->fibers[fiber_context->fiber_count];
    fiber_context->fiber_count += 1;
    return NULL;
}

// Poll results (and errors) are stored in the fibers.
static B_ERRFUNC
b_fibers_poll(
    struct B_Fiber *fibers,
    size_t fiber_count) {

    // Count poll items.
    size_t pollitem_total_count = 0;
    for (size_t i = 0; i < fiber_count; ++i) {
        struct B_Fiber const *fiber = &fibers[i];
        assert(fiber->poll);
        pollitem_total_count += fiber->poll->pollitem_count;
    }

    // Put poll items into a single array.
    // TODO(strager): Deduplicate file descriptors.
    zmq_pollitem_t pollitems[pollitem_total_count];
    {
        zmq_pollitem_t *cur_pollitem = pollitems;
        for (size_t i = 0; i < fiber_count; ++i) {
            struct B_Fiber const *fiber = &fibers[i];
            assert(fiber->poll);
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
        assert(fibers[i].poll->timeout_milliseconds < 0);
    }

    // Poll!
    const long timeout_milliseconds = -1;
    int rc = zmq_poll(
        pollitems,
        pollitem_total_count,
        timeout_milliseconds);

    struct B_Exception *poll_ex = NULL;
    int ready_pollitems = 0;
    if (rc == -1) {
        poll_ex = b_exception_errno("zmq_poll", rc);
    } else {
        ready_pollitems = rc;
    }

    bool const waken_all_fibers
        = poll_ex || (ready_pollitems == 0);

    // Copy results of polling to fibers.
    {
        int counted_ready_pollitems = 0;
        zmq_pollitem_t const *cur_pollitem = pollitems;
        for (
            size_t fiber_index = 0;
            fiber_index < fiber_count;
            ++fiber_index) {

            struct B_Fiber *fiber = &fibers[fiber_index];
            assert(fiber->poll);
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
    {
        size_t woken_fibers = 0;
        for (size_t i = 0; i < fiber_count; ++i) {
            if (!fibers[i].poll) {
                woken_fibers += 1;
            }
        }
        assert(woken_fibers > 0);
    }

    return NULL;
}

static void
b_fiber_context_switch(
    struct B_Fiber *fiber,
    struct B_FiberPoll *poll) {

    // Switch to the fiber, putting the current context
    // (fiber) in its place.
    assert(!fiber->poll);
    ucontext_t other_fiber_context;
    memcpy(
        &other_fiber_context,
        &fiber->context,
        sizeof(ucontext_t));

    fiber->poll = poll;
    // NOTE(strager): fiber (the pointer) is invalid after
    // calling swapcontext.
    int rc = swapcontext(
        &fiber->context,
        &other_fiber_context);
    assert(rc == 0);
}

static B_ERRFUNC
b_fiber_context_yield(
    struct B_FiberContext *fiber_context,
    struct B_FiberPoll *poll) {

    {
        struct B_Fiber *non_polling_fiber
            = b_fiber_context_get_non_polling_fiber(
                fiber_context);
        if (non_polling_fiber) {
            b_fiber_context_switch(non_polling_fiber, poll);
            return NULL;
        }
    }

    if (!poll) {
        // There are no other fibers, and we are not
        // polling, so there is nothing to do.
        return NULL;
    }

    // Poll for this fiber and all other fibers.
    struct B_Fiber *fiber;
    {
        struct B_Exception *ex
            = b_fiber_context_get_free_fiber(
                fiber_context,
                &fiber);
        if (ex) {
            return ex;
        }
    }

    // fiber->context is not used by b_fibers_poll.
    fiber->poll = poll;
    {
        struct B_Exception *ex = b_fibers_poll(
            fiber_context->fibers,
            fiber_context->fiber_count);
        if (ex) {
            return ex;
        }
    }

    // Remove the current fiber.
    assert(fiber == &fiber_context
        ->fibers[fiber_context->fiber_count - 1]);
    fiber_context->fiber_count -= 1;

    // Switch to a woken fiber.
    {
        struct B_Fiber *non_polling_fiber
            = b_fiber_context_get_non_polling_fiber(
                fiber_context);
        assert(non_polling_fiber);
        b_fiber_context_switch(non_polling_fiber, poll);
    }

    return NULL;
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
