#ifndef B_HEADER_GUARD_5691B0DF_6356_48C2_9F77_32D089B42C1E
#define B_HEADER_GUARD_5691B0DF_6356_48C2_9F77_32D089B42C1E

#if defined(__cplusplus)
# include <B/Error.h>

struct B_ErrorHandler;

// Abstract functor.
struct B_Deleter {
    B_ErrorHandler const *error_handler;

    B_Deleter(
            B_Deleter const &) = delete;
    B_Deleter &
    operator=(
            B_Deleter const &) = delete;

    // TODO(strager): Remove this constructor.
    B_Deleter() :
            error_handler(b_error_handler_cxx_throw()) {
    }

    B_Deleter(
            B_ErrorHandler const *error_handler) :
            error_handler(error_handler) {
    }

    B_Deleter(
            B_Deleter &&other) :
            error_handler(other.error_handler) {
    }

    B_Deleter &
    operator=(
            B_Deleter &&other) {
        if (this == &other) {
            return *this;
        }
        this->error_handler = other.error_handler;
        return *this;
    }
};
#endif

#endif
