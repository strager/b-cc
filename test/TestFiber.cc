#include <B/Fiber.h>

#include <gtest/gtest.h>

TEST(TestFiber, ForkNotExecutedImmediately) {
    struct B_Exception *ex;

    B_FiberContext *fiber_context;
    ex = b_fiber_context_allocate(&fiber_context);
    ASSERT_EQ(nullptr, ex);

    bool fork_executed = false;
    ex = b_fiber_context_fork(
        fiber_context,
        [](void *user_closure) -> void * {
            bool *fork_executed
                = static_cast<bool *>(user_closure);
            *fork_executed = true;
            return nullptr;
        },
        &fork_executed,
        nullptr);  // out_callback_result
    ASSERT_EQ(nullptr, ex);

    EXPECT_FALSE(fork_executed);

    ex = b_fiber_context_finish(fiber_context);
    ASSERT_EQ(nullptr, ex);

    EXPECT_TRUE(fork_executed);

    ex = b_fiber_context_deallocate(fiber_context);
    ASSERT_EQ(nullptr, ex);
}
