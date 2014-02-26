#include <B/Assert.h>
#include <B/Error.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

B_EXPORT enum B_ErrorHandlerResult
b_raise_error(
        struct B_ErrorHandler const *error_handler,
        struct B_Error error) {
    if (!error_handler) {
        error_handler = b_default_error_handler();
    }
    B_ASSERT(error_handler);

    enum B_ErrorHandlerResult result
        = error_handler->f(error_handler, error);
    switch (result) {
    case B_ERROR_ABORT:
    case B_ERROR_RETRY:
    case B_ERROR_IGNORE:
        return result;
    }

    fprintf(
        stderr,
        "b: ERROR: Invalid result from error handler; treating as B_ERROR_ABORT\n");
    return B_ERROR_ABORT;
}

B_EXPORT enum B_ErrorHandlerResult
b_raise_errno_error_impl(
        struct B_ErrorHandler const *error_handler,
        int errno_value) {
    return b_raise_error(error_handler, (struct B_Error) {
        .errno_value = errno_value,
    });
}

B_EXPORT bool
b_raise_precondition_error_impl(
        struct B_ErrorHandler const *error_handler) {
    return b_raise_error(error_handler, (struct B_Error) {
        .errno_value = EINVAL,  // FIXME(strager)
    });
}

static enum B_ErrorHandlerResult
default_error_handler_func_(
        struct B_ErrorHandler const *error_handler,
        struct B_Error error) {
    (void) error_handler;
    (void) fprintf(
        stderr,
        "b: ERROR: %s\n", strerror(error.errno_value));
    if (error.errno_value == EINTR) {
        return B_ERROR_RETRY;
    } else {
        return B_ERROR_ABORT;
    }
}

static struct B_ErrorHandler const
s_default_error_handler_ = {
    .f = default_error_handler_func_,
};

B_EXPORT struct B_ErrorHandler const *
b_default_error_handler(void) {
    return &s_default_error_handler_;
}
