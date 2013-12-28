#include <B/Exception.h>
#include <B/Internal/VTable.h>
#include <B/Internal/Validate.h>
#include <B/UUID.h>

#include <cstddef>
#include <memory>
#include <sstream>

static B_UUID
b_exception_cxx_uuid = B_UUID("F2414BCB-FA84-481A-9DDD-10CFDB2A5434");

static char const *
b_exception_cxx_allocate_message(
    B_Exception const *);

static void
b_exception_cxx_deallocate_message(
    char const *);

static void
b_exception_cxx_deallocate(
    B_Exception *);

B_DEFINE_VTABLE_FUNCTION(
    B_ExceptionVTable,
    b_exception_cxx_vtable,
    {
        .uuid = b_exception_cxx_uuid,
        .allocate_message = b_exception_cxx_allocate_message,
        .deallocate_message = b_exception_cxx_deallocate_message,
        .deallocate = b_exception_cxx_deallocate,
    })

struct B_ExceptionCXX {
    B_Exception ex;

    std::string message;
    std::unique_ptr<std::exception> exception;

    B_ExceptionCXX() :
        ex((B_Exception) {
            .vtable = b_exception_cxx_vtable(),
        }) {
    }
};

static B_ExceptionCXX *
b_exception_cxx_validate(
    B_Exception *);

static B_ExceptionCXX const *
b_exception_cxx_validate(
    B_Exception const *);

B_Exception *
b_exception_cxx(
    const char *message,
    std::unique_ptr<std::exception> &&exception) {

    auto ex = new B_ExceptionCXX();

    // TODO(strager): Move to constructor.
    ex->message = std::string(message);
    ex->exception = std::move(exception);

    return &ex->ex;
}

static char const *
b_exception_cxx_allocate_message(
    B_Exception const *ex_raw) {

    B_ExceptionCXX const *ex
        = b_exception_cxx_validate(ex_raw);
    return ex->message.c_str();
}

static void
b_exception_cxx_deallocate_message(
    char const *message) {

    (void) message;
}

static void
b_exception_cxx_deallocate(
    B_Exception *ex_raw) {

    B_ExceptionCXX *ex
        = b_exception_cxx_validate(ex_raw);
    delete ex;
}

static B_ExceptionCXX const *
b_exception_cxx_validate(
    B_Exception const *ex) {

    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(
        ex->vtable->uuid,
        b_exception_cxx_uuid));

    _Static_assert(
        offsetof(B_ExceptionCXX, ex) == 0,
        "ex must be the first member of struct B_ExceptionCXX");
    return reinterpret_cast<B_ExceptionCXX const *>(ex);
}

static B_ExceptionCXX *
b_exception_cxx_validate(
    B_Exception *ex) {

    return const_cast<B_ExceptionCXX *>(
        b_exception_cxx_validate(ex));
}
