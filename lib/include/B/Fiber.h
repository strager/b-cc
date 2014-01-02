#ifndef FIBER_H_0B73ADA8_E982_418F_94A1_772973CED23E
#define FIBER_H_0B73ADA8_E982_418F_94A1_772973CED23E

#include <B/Internal/Common.h>

#include <zmq.h>

#ifdef __cplusplus
extern "C" {
#endif

// FiberContexts are *not* thread-safe.  Up to one
// FiberContext per thread is recommended.
//
// Fibers are created using b_fiber_context_fork.  Each
// fiber runs until it encounters b_fiber_context_poll_zmq
// (which causes a 'yield').  When all fiber threads are in
// b_fiber_context_poll_zmq, zmq_poll is called (with all of
// the zmq_pollitem_t of each b_fiber_context_poll_zmq call)
// and fibers are awoken.

struct B_FiberContext;

B_ERRFUNC
b_fiber_context_allocate(
    struct B_FiberContext **out);

B_ERRFUNC
b_fiber_context_deallocate(
    struct B_FiberContext *);

B_ERRFUNC
b_fiber_context_finish(
    struct B_FiberContext *);

B_ERRFUNC
b_fiber_context_poll_zmq(
    struct B_FiberContext *,
    zmq_pollitem_t *pollitems,
    int pollitem_count,
    long timeout_milliseconds,
    int *ready_pollitems,
    bool *is_finished);

B_ERRFUNC
b_fiber_context_fork(
    struct B_FiberContext *,
    void *(*callback)(void *user_closure),
    void *user_closure,
    void **out_callback_result);

B_ERRFUNC
b_fiber_context_soft_yield(
    struct B_FiberContext *);

// Yields and waits for one of the following conditions:
//
// * This fiber is the only fiber for the context, or
// * At least one fiber woke up from a
//   b_fiber_context_poll_zmq call.
B_ERRFUNC
b_fiber_context_hard_yield(
    struct B_FiberContext *);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
#include <type_traits>

template<
    typename TFunc,
    typename TFuncResult = typename std::result_of<TFunc()>::type,
    typename = typename std::enable_if<
        std::is_pointer<TFuncResult>::value>::type>
B_ERRFUNC
b_fiber_context_fork(
    B_FiberContext *fiber_context,
    TFunc const &thread_function,
    TFuncResult *out_callback_result) {

    return b_fiber_context_fork(
        fiber_context,
        [](void *user_closure) -> void * {
            TFunc const &hread_function
                = *static_cast<TFunc const *>(user_closure);
            return thread_function();
        },
        const_cast<void *>(
            static_cast<void const *>(&thread_function)),
        static_cast<void **>(out_callback_result));
}

template<
    typename TFunc,
    typename = typename
        std::enable_if<
            std::is_void<
                typename std::result_of<TFunc()> ::type
            >::value>
        ::type>
B_ERRFUNC
b_fiber_context_fork(
    B_FiberContext *fiber_context,
    TFunc const &thread_function) {

    return b_fiber_context_fork(
        fiber_context,
        [](void *user_closure) -> void * {
            TFunc const &thread_function
                = *static_cast<TFunc const *>(user_closure);
            thread_function();
            return nullptr;
        },
        const_cast<void *>(
            static_cast<void const *>(&thread_function)),
        nullptr);
}
#endif

#endif
