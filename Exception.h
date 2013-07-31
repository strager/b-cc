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

#endif
