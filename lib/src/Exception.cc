#include <B/Exception.h>
#include <B/Portable.h>
#include <B/UUID.h>
#include <B/Validate.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

static B_UUID
b_exception_string_uuid = B_UUID("AC32D2DB-3201-4FB8-8CBB-F9F294203F30");

static void
b_exception_string_deallocate(
    B_Exception *ex) {
    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(ex->uuid, b_exception_string_uuid));

    free((char *) ex->message);
    delete ex;
}

static B_UUID
b_exception_aggregate_uuid = B_UUID("4B332F25-4DF4-4B19-846E-3610209367FE");

static void
b_exception_aggregate_deallocate(
    B_Exception *ex) {
    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(ex->uuid, b_exception_aggregate_uuid));

    auto sub_exceptions
        = static_cast<B_Exception **>(ex->data);
    B_VALIDATE(sub_exceptions);

    for (auto sub_ex = sub_exceptions; *sub_ex; ++sub_ex) {
        b_exception_deallocate(*sub_ex);
    }

    delete sub_exceptions;
    free((char *) ex->message);
    delete ex;
}

static B_UUID
b_exception_cxx_uuid = B_UUID("F2414BCB-FA84-481A-9DDD-10CFDB2A5434");

static void
b_exception_cxx_deallocate(
    B_Exception *ex) {
    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(ex->uuid, b_exception_cxx_uuid));

    free((char *) ex->message);
    delete static_cast<std::exception *>(ex->data);
    delete ex;
}

B_Exception *
b_exception_string(
    const char *message) {
    return new B_Exception {
        .uuid = b_exception_string_uuid,
        .message = b_strdup(message),
        .data = NULL,
        .deallocate = b_exception_string_deallocate,
    };
}

B_Exception *
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

    return new B_Exception {
        .uuid = b_exception_string_uuid,
        .message = message,
        .data = NULL,
        .deallocate = b_exception_string_deallocate,
    };
}

struct B_Exception *
b_exception_errno(
    const char *function,
    int errno_) {
    return b_exception_format_string(
        "%s: %s",
        function,
        strerror(errno));
}

void
b_exception_deallocate(
    B_Exception *ex) {
    b_exception_validate(ex);
    ex->deallocate(ex);
}

B_Exception *
b_exception_aggregate(
    B_Exception **source_exceptions,
    size_t count) {
    std::ostringstream message;
    message << "Aggregate exception:";

    // Create a NULL-terminated list of exceptions from
    // source_exceptions.
    auto sub_exceptions = std::unique_ptr<B_Exception *>(
        new B_Exception *[count + 1]);
    size_t sub_exception_index = 0;
    for (size_t i = 0; i < count; ++i) {
        if (B_Exception *source_ex = source_exceptions[i]) {
            b_exception_validate(source_ex);
            sub_exceptions.get()[sub_exception_index++] = source_ex;
            message << "\n - " << source_ex->message;
        }
    }

    if (sub_exception_index == 0) {
        // No exceptions given.
        // No exception returned.
        // No exceptions.
        return nullptr;
    } else if (sub_exception_index == 1) {
        // One exception.
        // One exception returned.
        // No exceptions.
        return *sub_exceptions;
    } else {
        sub_exceptions.get()[sub_exception_index] = nullptr;
        return new B_Exception {
            .uuid = b_exception_aggregate_uuid,
            .message = b_strdup(message.str().c_str()),
            .data = sub_exceptions.release(),
            .deallocate = b_exception_aggregate_deallocate,
        };
    }
}

void
b_exception_validate(
    B_Exception *ex) {
    B_VALIDATE(ex);
    b_uuid_validate(ex->uuid);
    B_VALIDATE(ex->message);
    B_VALIDATE(ex->deallocate);
}

B_Exception *
b_exception_cxx(
    const char *message,
    std::unique_ptr<std::exception> &&exception) {
    return new B_Exception {
        .uuid = b_exception_cxx_uuid,
        .message = b_strdup(message),
        .data = exception.release(),
        .deallocate = b_exception_cxx_deallocate,
    };
}
