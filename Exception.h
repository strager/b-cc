#ifndef EXCEPTION_H_9451BAF7_374C_4B0F_83D2_FCEC16055154
#define EXCEPTION_H_9451BAF7_374C_4B0F_83D2_FCEC16055154

#include "UUID.h"

// An Exception is an abstract C++-style exception.  C++,
// Objective-C, POSIX, and other kinds of exception and
// error types may be encoded in a B_Exception type.
struct B_Exception {
    struct B_UUID uuid;
    const char *message;
    void *data;
    void (*deallocate)(struct B_Exception *);
};

// Creates a plain-text Exception.
struct B_Exception *
b_exception_allocate(const char *message);

// Destroys any type of Exception.
void
b_exception_deallocate(struct B_Exception *);

// Rethrows an exception if an exception occured, and
// returns from this function.  Example usage:
//
// void
// my_function(/* ..., */ struct B_Exception **ex) {
//     other_function(/* ..., */ ex);
//     B_RETHROW_EXCEPTION(ex);
//     /* ... */
// }
#define B_RETHROW_EXCEPTION(ex_var) \
    B_RETHROW_EXCEPTION_RETURNING(ex_var, /* no return value */)

// Rethrows an exception if an exception occured, and
// returns from this function with the specified value.
// Example usage:
//
// int
// my_function(/* ..., */ struct B_Exception **ex) {
//     other_function(/* ..., */ ex);
//     B_RETHROW_EXCEPTION_RETURNING(ex, 0);
//     /* ... */
// }
#define B_RETHROW_EXCEPTION_RETURNING(ex_var, return_value) \
    do { \
        if (*(ex_var)) { \
            return return_value; \
        } \
    } while (0)

#endif
