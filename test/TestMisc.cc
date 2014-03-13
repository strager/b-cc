#include <B/Alloc.h>
#include <B/Misc.h>

#include <gtest/gtest.h>

TEST(TestMisc, TestDupArgs) {
    // NOTE(strager): valgrind checks for memory leaks in
    // Travis CI.
    B_ErrorHandler const *eh = nullptr;

    {
        char echo[] = "echo";
        char hello[] = "hello";
        char world[] = "world";
        char const *args[] = {
            echo,
            hello,
            world,
            nullptr,
        };

        char const *const *out_args;
        ASSERT_TRUE(b_dup_args(args, &out_args, eh));
        ASSERT_NE(out_args, nullptr);

        // Trash args in case out_args refers to args.
        echo[0] = '#';
        hello[0] = '#';
        world[0] = '#';
        args[0] = nullptr;
        args[1] = nullptr;
        args[2] = nullptr;

        EXPECT_STREQ("echo", out_args[0]);
        EXPECT_STREQ("hello", out_args[1]);
        EXPECT_STREQ("world", out_args[2]);
        EXPECT_EQ(nullptr, out_args[3]);
        EXPECT_TRUE(b_deallocate((void *) out_args, eh));
    }
}
