#include <B/Error.h>

static B_ErrorHandler
s_error_handler_cxx_throw_ = {
    [](
            B_ErrorHandler const *,
            B_Error error) -> B_ErrorHandlerResult {
        throw B_ErrorException(error);
    },
};

B_ErrorHandler const *
b_error_handler_cxx_throw() {
    return &s_error_handler_cxx_throw_;
}
