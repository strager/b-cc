#include <B/Exception.h>
#include <B/Internal/VTable.h>
#include <B/UUID.h>

static struct B_UUID
b_exception_memory_uuid = B_UUID("B05CAAE7-5E12-4D06-ACC9-6E17E019FBF4");

static char const *
b_exception_memory_allocate_message(
    struct B_Exception const *);

static void
b_exception_memory_deallocate_message(
    char const *);

static void
b_exception_memory_deallocate(
    struct B_Exception *);

B_DEFINE_VTABLE_FUNCTION(
    struct B_ExceptionVTable,
    b_exception_memory_vtable,
    {
        .uuid = b_exception_memory_uuid,
        .allocate_message = b_exception_memory_allocate_message,
        .deallocate_message = b_exception_memory_deallocate_message,
        .deallocate = b_exception_memory_deallocate,
    })

static char const *
b_exception_memory_allocate_message(
    struct B_Exception const *ex) {

    (void) ex;
    return "out of memory";
}

static void
b_exception_memory_deallocate_message(
    char const *message) {

    (void) message;
}

static void
b_exception_memory_deallocate(
    struct B_Exception *ex) {

    (void) ex;
}

struct B_Exception *
b_exception_memory(
    void) {

    static struct B_Exception singleton_memory_exception;
    // FIXME(strager): Make thread-safe.
    singleton_memory_exception = (struct B_Exception) {
        .vtable = b_exception_memory_vtable(),
    };

    return &singleton_memory_exception;
}
