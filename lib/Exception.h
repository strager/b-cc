#ifndef EXCEPTION_H_9451BAF7_374C_4B0F_83D2_FCEC16055154
#define EXCEPTION_H_9451BAF7_374C_4B0F_83D2_FCEC16055154

#include "UUID.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// An Exception is an abstract C++-style exception.  C++,
// Objective-C, POSIX, and other kinds of exception and
// error types may be encoded in a B_Exception type.
struct B_Exception {
    struct B_UUID uuid;
    const char *message;
    void *data;
    void (*deallocate)(struct B_Exception *);
};

// Creates a plain-text Exception with the given message.
struct B_Exception *
b_exception_string(
    const char *message);

// Creates a plain-text Exception from a printf-like format
// specifier.
struct B_Exception *
b_exception_format_string(
    const char *format,
    ...);

// Creates a plain-text Exception from a POSIX error code.
struct B_Exception *
b_exception_errno(
    const char *function,
    int errno);

// Destroys any type of Exception.
void
b_exception_deallocate(
    struct B_Exception *);

// Creates an Exception which is the combination of multiple
// exceptions.  Deallocating the returned Exception will
// deallocate the aggregated exceptions.
struct B_Exception *
b_exception_aggregate(
    struct B_Exception **,
    size_t count);

void
b_exception_validate(
    struct B_Exception *);

#define B_EXCEPTION_THEN(ex_var, ...) \
    do { \
        if (*(ex_var)) { \
            __VA_ARGS__ \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <exception>
#include <memory>
#include <stdexcept>
#include <type_traits>

B_Exception *
b_exception_cxx(
    const char *message,
    std::unique_ptr<std::exception> &&);

#define B_RESULT_OF_EXCEPTION_FUNC \
    typename std::result_of<TFunc(TArgs...)>::type
template<typename TFunc, typename ...TArgs>
B_RESULT_OF_EXCEPTION_FUNC
b_exception_try_cxx(
    TFunc func,
    B_Exception **ex,
    TArgs... args) {
    typedef B_RESULT_OF_EXCEPTION_FUNC result_type;

#define B_RETHROW_STD_EXCEPTION(type) \
    catch (const type &exception) { \
        *ex = b_exception_cxx( \
            exception.what(), \
            std::unique_ptr<type>(new type(exception))); \
        return result_type(); \
    }

    try {
        return func(args...);
    }

    // <stdexcept>
    B_RETHROW_STD_EXCEPTION(std::domain_error    )  // std::logic_error
    B_RETHROW_STD_EXCEPTION(std::invalid_argument)  // std::logic_error
    B_RETHROW_STD_EXCEPTION(std::length_error    )  // std::logic_error
    B_RETHROW_STD_EXCEPTION(std::out_of_range    )  // std::logic_error
    B_RETHROW_STD_EXCEPTION(std::logic_error     )  // std::exception

    B_RETHROW_STD_EXCEPTION(std::range_error     )  // std::runtime_error
    B_RETHROW_STD_EXCEPTION(std::overflow_error  )  // std::runtime_error
    B_RETHROW_STD_EXCEPTION(std::underflow_error )  // std::runtime_error
    B_RETHROW_STD_EXCEPTION(std::runtime_error   )  // std::exception

    // <exception>
    B_RETHROW_STD_EXCEPTION(std::bad_exception)  // std::exception
    B_RETHROW_STD_EXCEPTION(std::exception)

    catch (B_Exception *exception) {
        *ex = exception;
        return result_type();
    } catch (...) {
        *ex = b_exception_string("Unknown C++ exception");
        return result_type();
    }
}
#undef B_RESULT_OF_EXCEPTION_FUNC
#endif

#endif
