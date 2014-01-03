#include <B/Fiber.h>
#include <B/Internal/GTest.h>

#include <gtest/gtest.h>
#include <zmq.h>

#include <poll.h>

TEST(TestFiber, ForkNotExecutedImmediately) {
    B_FiberContext *fiber_context;
    B_CHECK_EX(b_fiber_context_allocate(&fiber_context));

    bool volatile fork_executed = false;
    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [&fork_executed]() {
            fork_executed = true;
        }));

    EXPECT_FALSE(fork_executed);

    B_CHECK_EX(b_fiber_context_finish(fiber_context));

    EXPECT_TRUE(fork_executed);

    B_CHECK_EX(b_fiber_context_deallocate(fiber_context));
}

TEST(TestFiber, YieldAwakensPoll) {
    B_FiberContext *fiber_context;
    B_CHECK_EX(b_fiber_context_allocate(&fiber_context));

    int fds[2];
    ASSERT_EQ(0, pipe(fds))
        << "Errno: " << errno;

    size_t volatile poll_calls = 0;
    bool volatile should_stop = false;
    bool volatile fork_finished = false;
    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [&]() {
            zmq_pollitem_t pollitems[] = {
                { nullptr, fds[0], POLLIN, 0 },
            };
            while (!should_stop) {
                long const timeout_milliseconds = -1;

                int ready_pollitems;
                poll_calls += 1;
                B_CHECK_EX(b_fiber_context_poll_zmq(
                    fiber_context,
                    pollitems,
                    sizeof(pollitems) / sizeof(*pollitems),
                    timeout_milliseconds,
                    &ready_pollitems,
                    nullptr));
                EXPECT_EQ(0, ready_pollitems);
            }

            fork_finished = true;
        }));

    EXPECT_EQ(0, poll_calls);

    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(1, poll_calls);

    should_stop = true;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(1, poll_calls);
    EXPECT_TRUE(fork_finished);

    B_CHECK_EX(b_fiber_context_deallocate(fiber_context));
}

TEST(TestFiber, ThreeYieldingForksFairlyScheduled) {
    B_FiberContext *fiber_context;
    B_CHECK_EX(b_fiber_context_allocate(&fiber_context));

    int volatile main_scheduled = 0;
    int volatile fork1_scheduled = 0;
    int volatile fork2_scheduled = 0;

    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [fiber_context, &fork1_scheduled]() {
            ++fork1_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork1_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork1_scheduled;
        }));

    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [fiber_context, &fork2_scheduled]() {
            ++fork2_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork2_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork2_scheduled;
        }));

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(1, main_scheduled);
    EXPECT_EQ(1, fork1_scheduled);
    EXPECT_EQ(1, fork2_scheduled);

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(2, main_scheduled);
    EXPECT_EQ(2, fork1_scheduled);
    EXPECT_EQ(2, fork2_scheduled);

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(3, main_scheduled);
    EXPECT_EQ(3, fork1_scheduled);
    EXPECT_EQ(3, fork2_scheduled);

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(4, main_scheduled);
    EXPECT_EQ(3, fork1_scheduled);
    EXPECT_EQ(3, fork2_scheduled);

    B_CHECK_EX(b_fiber_context_deallocate(fiber_context));
}

TEST(TestFiber, FourYieldingForksFairlyScheduled) {
    B_FiberContext *fiber_context;
    B_CHECK_EX(b_fiber_context_allocate(&fiber_context));

    int volatile main_scheduled = 0;
    int volatile fork1_scheduled = 0;
    int volatile fork2_scheduled = 0;
    int volatile fork3_scheduled = 0;

    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [fiber_context, &fork1_scheduled]() {
            ++fork1_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork1_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork1_scheduled;
        }));

    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [fiber_context, &fork2_scheduled]() {
            ++fork2_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork2_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork2_scheduled;
        }));

    B_CHECK_EX(b_fiber_context_fork(
        fiber_context,
        [fiber_context, &fork3_scheduled]() {
            ++fork3_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork3_scheduled;

            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
            ++fork3_scheduled;
        }));

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(1, main_scheduled);
    EXPECT_EQ(1, fork1_scheduled);
    EXPECT_EQ(1, fork2_scheduled);
    EXPECT_EQ(1, fork3_scheduled);

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(2, main_scheduled);
    EXPECT_EQ(2, fork1_scheduled);
    EXPECT_EQ(2, fork2_scheduled);
    EXPECT_EQ(2, fork3_scheduled);

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(3, main_scheduled);
    EXPECT_EQ(3, fork1_scheduled);
    EXPECT_EQ(3, fork2_scheduled);
    EXPECT_EQ(3, fork3_scheduled);

    ++main_scheduled;
    B_CHECK_EX(b_fiber_context_hard_yield(fiber_context));
    EXPECT_EQ(4, main_scheduled);
    EXPECT_EQ(3, fork1_scheduled);
    EXPECT_EQ(3, fork2_scheduled);
    EXPECT_EQ(3, fork3_scheduled);

    B_CHECK_EX(b_fiber_context_deallocate(fiber_context));
}
