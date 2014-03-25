#include <B/Error.h>

#include <functional>
#include <gtest/gtest.h>

struct FunctionErrorHandler_ :
        public B_ErrorHandler {
    typedef B_ErrorHandlerResult FunctionType(B_Error);

    explicit
    FunctionErrorHandler_(
            std::function<FunctionType> func) :
            B_ErrorHandler{error_handler_func},
            func(func) {
    }

    std::function<FunctionType> func;

private:
    static B_ErrorHandlerResult
    error_handler_func(
            B_ErrorHandler const *error_handler,
            B_Error error) {
        return static_cast<FunctionErrorHandler_ const *>(
            error_handler)->func(error);
    }
};

TEST(TestErrorHandler, RaiseCallsErrorHandler)
{
    size_t call_count = 0;
    FunctionErrorHandler_ error_handler([&](
            B_Error error) {
        call_count += 1;
        EXPECT_EQ(EINVAL, error.errno_value);
        return B_ERROR_RETRY;
    });

    EXPECT_EQ(B_ERROR_RETRY, b_raise_error(
        &error_handler,
        B_Error{EINVAL}));
    EXPECT_EQ(1U, call_count);
}
