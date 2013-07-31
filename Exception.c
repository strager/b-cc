#include "Allocate.h"
#include "Exception.h"
#include "Portable.h"
#include "UUID.h"
#include "Validate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct B_UUID
b_exception_string_uuid = B_UUID("AC32D2DB-3201-4FB8-8CBB-F9F294203F30");

static void
b_exception_string_deallocate(
    struct B_Exception *ex) {
    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(ex->uuid, b_exception_string_uuid));

    free((char *) ex->message);
    free(ex);
}

static struct B_UUID
b_exception_aggregate_uuid = B_UUID("4B332F25-4DF4-4B19-846E-3610209367FE");

static void
b_exception_aggregate_deallocate(
    struct B_Exception *ex) {
    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(ex->uuid, b_exception_aggregate_uuid));

    struct B_Exception **sub_exceptions
        = (struct B_Exception **) ex->data;
    B_VALIDATE(sub_exceptions);

    for (
        struct B_Exception **sub_ex = sub_exceptions;
        *sub_ex;
        ++sub_ex) {
        free(*sub_ex);
    }

    free(sub_exceptions);
    free((char *) ex->message);
    free(ex);
}

struct B_Exception *
b_exception_string(
    const char *message) {
    B_ALLOCATE(struct B_Exception, ex, {
        .uuid = b_exception_string_uuid,
        .message = b_strdup(message),
        .data = NULL,
        .deallocate = b_exception_string_deallocate,
    });
    return ex;
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
        B_VALIDATE(err >= 0);
        va_end(vp);
    }

    B_ALLOCATE(struct B_Exception, ex, {
        .uuid = b_exception_string_uuid,
        .message = message,
        .data = NULL,
        .deallocate = b_exception_string_deallocate,
    });
    return ex;
}

struct B_Exception *
b_exception_errno(
    const char *function,
    int errno) {
    return b_exception_format_string(
        "%s: %s",
        function,
        strerror(errno));
}

void
b_exception_deallocate(
    struct B_Exception *ex) {
    b_exception_validate(ex);
    ex->deallocate(ex);
}

struct B_Exception *
b_exception_aggregate(
    struct B_Exception **source_exceptions,
    size_t count) {
    static const char header[] = "Aggregate exception:";
    static const size_t header_length = sizeof(header) - 1;
    static const char ex_prefix[] = "\n - ";
    static const size_t ex_prefix_length = sizeof(ex_prefix) - 1;

    // Copy exception pointers into NULL-terminated list and
    // calculate length of message.
    size_t message_length = header_length;
    struct B_Exception **sub_exceptions
        = malloc(sizeof(struct B_Exception) * (count + 1));
    struct B_Exception **sub_ex = sub_exceptions;
    for (size_t i = 0; i < count; ++i) {
        struct B_Exception *source_ex = source_exceptions[i];
        if (source_ex) {
            b_exception_validate(source_ex);
            message_length += ex_prefix_length + strlen(source_ex->message);
            *sub_ex++ = source_ex;
        }
    }
    *sub_ex = NULL;

    char *message = malloc(message_length + 1);
    strcpy(message, header);
    for (sub_ex = sub_exceptions; *sub_ex; ++sub_ex) {
        // FIXME Horrible time complexity.  Should use
        // stpcpy, or (of course) a manual copy for
        // portability.
        strcat(message, ex_prefix);
        strcat(message, (*sub_ex)->message);
    }

    B_ALLOCATE(struct B_Exception, ex, {
        .uuid = b_exception_aggregate_uuid,
        .message = message,
        .data = sub_exceptions,
        .deallocate = b_exception_aggregate_deallocate,
    });
    return ex;
}

void
b_exception_validate(
    struct B_Exception *ex) {
    B_VALIDATE(ex);
    b_uuid_validate(ex->uuid);
    B_VALIDATE(ex->message);
    B_VALIDATE(ex->deallocate);
}
