#ifndef B_HEADER_GUARD_CE4E98DC_9A5C_4937_A3B7_FE602A3472B7
#define B_HEADER_GUARD_CE4E98DC_9A5C_4937_A3B7_FE602A3472B7

#include <B/Base.h>

struct B_Error;
struct B_ErrorHandler;

enum B_ErrorHandlerResult {
    B_ERROR_ABORT  = 1,
    B_ERROR_RETRY  = 2,
    B_ERROR_IGNORE = 3,
};

struct B_Error {
    int errno_value;
};

typedef enum B_ErrorHandlerResult
B_ErrorHandlerFunc(
        struct B_ErrorHandler const *,
        struct B_Error);

struct B_ErrorHandler {
    B_ErrorHandlerFunc *f;
};

#define B_RAISE_ERRNO_ERROR(_eh, _errno_value, _context) \
    (b_raise_errno_error_impl((_eh), (_errno_value)))

#define B_RAISE_PRECONDITION_ERROR(_eh, _false_condition) \
    (b_raise_precondition_error_impl((_eh)))

#define B_CHECK_PRECONDITION(_eh, _cond) \
    do { \
        if (!(_cond)) { \
            return B_RAISE_PRECONDITION_ERROR( \
                (_eh), \
                #_cond); \
        } \
    } while (0)

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT enum B_ErrorHandlerResult
b_raise_error(
        struct B_ErrorHandler const *,
        struct B_Error);

B_EXPORT enum B_ErrorHandlerResult
b_raise_errno_error_impl(
        struct B_ErrorHandler const *,
        int errno_value);

// Always returns false.
B_EXPORT bool
b_raise_precondition_error_impl(
        struct B_ErrorHandler const *);

// If an ErrorHandler is NULL and an Error needs to be
// handled, this function is called.
B_EXPORT struct B_ErrorHandler const *
b_default_error_handler(void);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <string.h>
// Throws an ErrorException if an error occurs.  Pretty
// unsafe.
B_ErrorHandler const *
b_error_handler_cxx_throw();

class B_ErrorException :
        public std::exception {
public:
    B_ErrorException(
            B_Error error) noexcept :
            error(error) {
    }

    B_ErrorException(
            B_ErrorException const &other) noexcept :
            B_ErrorException(other.error) {
    }

    B_ErrorException(
            B_ErrorException &&other) noexcept :
            B_ErrorException(other.error) {
    }

    virtual ~B_ErrorException() noexcept {
    }

    B_ErrorException &
    operator=(
            B_ErrorException const &other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->error = other.error;
        return *this;
    }

    B_ErrorException &
    operator=(
            B_ErrorException &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->error = other.error;
        return *this;
    }

    char const *
    what() const noexcept override {
        // FIXME(strager): Use of strerror is discouraged.
        return strerror(this->error.errno_value);
    }

    B_Error error;
};
#endif

#endif
