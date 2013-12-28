#include <B/Exception.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Portable.h>
#include <B/Internal/VTable.h>
#include <B/Internal/Validate.h>
#include <B/UUID.h>

#include <zmq.h>

#include <assert.h>
#include <stddef.h>

static struct B_UUID
b_exception_errno_uuid = B_UUID("160EEE36-0217-4C19-9C4F-890011328893");

static char const *
b_exception_errno_allocate_message(
    struct B_Exception const *);

static void
b_exception_errno_deallocate_message(
    char const *);

static void
b_exception_errno_deallocate(
    struct B_Exception *);

B_DEFINE_VTABLE_FUNCTION(
    struct B_ExceptionVTable,
    b_exception_errno_vtable,
    {
        .uuid = b_exception_errno_uuid,
        .allocate_message = b_exception_errno_allocate_message,
        .deallocate_message = b_exception_errno_deallocate_message,
        .deallocate = b_exception_errno_deallocate,
    })

struct B_ExceptionErrno {
    struct B_Exception ex;

    char *function_name;
    int errno_value;
};

static struct B_ExceptionErrno const *
b_exception_errno_validate(
    struct B_Exception const *);

struct B_Exception *
b_exception_errno(
    char const *function,
    int errno_value) {

    B_ALLOCATE(struct B_ExceptionErrno, ex, {
        .ex = {
            .vtable = b_exception_errno_vtable(),
        },
        .function_name = b_strdup(function),
        .errno_value = errno_value,
    });
    return &ex->ex;
}

static char const *
b_exception_errno_allocate_message(
    struct B_Exception const *ex_raw) {

    struct B_ExceptionErrno const *ex
        = b_exception_errno_validate(ex_raw);

    char *message = NULL;
    {
        int rc = asprintf(
            &message,
            "%s: %s (%d)",
            ex->function_name,
            zmq_strerror(ex->errno_value),
            ex->errno_value);
        assert(rc > 0);  // FIXME(strager)
    }

    return message;
}

static void
b_exception_errno_deallocate_message(
    char const *message) {

    free((char *) message);
}

static void
b_exception_errno_deallocate(
    struct B_Exception *ex_raw) {

    struct B_ExceptionErrno *ex
        = (void *) b_exception_errno_validate(ex_raw);
    free(ex->function_name);
    free(ex);
}

static struct B_ExceptionErrno const *
b_exception_errno_validate(
    struct B_Exception const *ex) {

    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(
        ex->vtable->uuid,
        b_exception_errno_uuid));

    _Static_assert(
        offsetof(struct B_ExceptionErrno, ex) == 0,
        "ex must be the first member of struct B_ExceptionErrno");
    return (struct B_ExceptionErrno const *) ex;
}
