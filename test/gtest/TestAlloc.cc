#include <B/Alloc.h>

#include <gtest/gtest.h>

TEST(TestAlloc, Strdupplus) {
    B_ErrorHandler const *eh = nullptr;
    char *string = nullptr;
    ASSERT_TRUE(b_strdupplus("hello", 6, &string, eh));
    ASSERT_NE(nullptr, string);
    EXPECT_STREQ("hello", string);
    strcat(string, " world");  // Shouldn't trip Valgrind.
    EXPECT_STREQ("hello world", string);
    EXPECT_TRUE(b_deallocate(string, eh));
}
