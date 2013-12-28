#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/Allocate.h>

#include <poll.h>

struct B_FiberContext {
    // TODO(strager)
};

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
