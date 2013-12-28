#include <B/Exception.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Portable.h>
#include <B/Internal/VTable.h>
#include <B/Internal/Validate.h>
#include <B/UUID.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static struct B_UUID
b_exception_string_uuid = B_UUID("AC32D2DB-3201-4FB8-8CBB-F9F294203F30");

static char const *
b_exception_string_allocate_message(
    struct B_Exception const *);

static void
b_exception_string_deallocate_message(
    char const *);

static void
b_exception_string_deallocate(
    struct B_Exception *);

B_DEFINE_VTABLE_FUNCTION(
    struct B_ExceptionVTable,
    b_exception_string_vtable,
    {
        .uuid = b_exception_string_uuid,
        .allocate_message = b_exception_string_allocate_message,
        .deallocate_message = b_exception_string_deallocate_message,
        .deallocate = b_exception_string_deallocate,
    })

struct B_ExceptionString {
    struct B_Exception ex;

    char *message;
};

static struct B_ExceptionString const *
b_exception_string_validate(
    struct B_Exception const *);

static struct B_Exception *
b_exception_string_no_copy(
    char *);

struct B_Exception *
b_exception_string(
    char const *message) {

    return b_exception_string_no_copy(b_strdup(message));
}

struct B_Exception *
b_exception_format_string(
    const char *format,
    ...) {

    char *message = NULL;
    {
        va_list vp;
        va_start(vp, format);
        int err = vasprintf(&message, format, vp);
        B_VALIDATE(err >= 0);  // FIXME(strager)
        va_end(vp);
    }

    return b_exception_string_no_copy(message);
}

static char const *
b_exception_string_allocate_message(
    struct B_Exception const *ex_raw) {

    struct B_ExceptionString const *ex
        = b_exception_string_validate(ex_raw);
    return ex->message;
}

static void
b_exception_string_deallocate_message(
    char const *message) {

    (void) message;
}

static void
b_exception_string_deallocate(
    struct B_Exception *ex_raw) {

    struct B_ExceptionString *ex
        = (void *) b_exception_string_validate(ex_raw);
    free(ex->message);
    free(ex);
}

static struct B_ExceptionString const *
b_exception_string_validate(
    struct B_Exception const *ex) {

    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(
        ex->vtable->uuid,
        b_exception_string_uuid));

    _Static_assert(
        offsetof(struct B_ExceptionString, ex) == 0,
        "ex must be the first member of struct B_ExceptionString");
    return (struct B_ExceptionString const *) ex;
}

static struct B_Exception *
b_exception_string_no_copy(
    char *message) {

    B_ALLOCATE(struct B_ExceptionString, ex, {
        .ex = {
            .vtable = b_exception_string_vtable(),
        },
        .message = message,
    });
    return &ex->ex;
}
