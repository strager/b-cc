#include <B/Exception.h>

#include <gtest/gtest.h>

TEST(TestException, StringMessageMatchesOriginal) {
    char message[] = "hello world";
    struct B_Exception *ex = b_exception_string(message);
    message[0] = 'x';

    char const *ex_message
        = b_exception_allocate_message(ex);
    EXPECT_STREQ("hello world", ex_message);
    b_exception_deallocate_message(ex, ex_message);

    b_exception_deallocate(ex);
}
