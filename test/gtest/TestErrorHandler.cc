#include <B/Error.h>

#include <errno.h>
#include <gtest/gtest.h>

TEST(TestErrorHandler, RaiseCallsErrorHandler)
{
    struct TestErrorHandler_ : public B_ErrorHandler {
        TestErrorHandler_(
                B_ErrorHandlerFunc *f) :
                B_ErrorHandler{f},
                call_count(0) {
        }
        size_t mutable call_count;
    };
    TestErrorHandler_ error_handler([](
            B_ErrorHandler const *eh,
            B_Error error) -> B_ErrorHandlerResult {
        static_cast<TestErrorHandler_ const *>(eh)
            ->call_count += 1;
        EXPECT_EQ(EINVAL, error.errno_value);
        return B_ERROR_RETRY;
    });

    EXPECT_EQ(B_ERROR_RETRY, b_raise_error(
        &error_handler,
        B_Error{EINVAL}));
    EXPECT_EQ(1U, error_handler.call_count);
}
