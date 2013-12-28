#include <B/Exception.h>
#include <B/Internal/Validate.h>
#include <B/UUID.h>

void
b_exception_deallocate(
    struct B_Exception *ex) {

    b_exception_validate(ex);
    ex->vtable->deallocate(ex);
}

char const *
b_exception_allocate_message(
    struct B_Exception const *ex) {

    b_exception_validate(ex);

    return ex->vtable->allocate_message(ex);
}

void
b_exception_deallocate_message(
    struct B_Exception const *ex,
    char const *message) {

    b_exception_validate(ex);

    return ex->vtable->deallocate_message(message);
}

void
b_exception_validate(
    struct B_Exception const *ex) {

    B_VALIDATE(ex);
    B_VALIDATE(ex->vtable);
    b_uuid_validate(ex->vtable->uuid);
    B_VALIDATE(ex->vtable->allocate_message);
    B_VALIDATE(ex->vtable->deallocate_message);
    B_VALIDATE(ex->vtable->deallocate);
}
