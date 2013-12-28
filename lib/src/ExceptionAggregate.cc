#include <B/Exception.h>
#include <B/Internal/Portable.h>
#include <B/Internal/VTable.h>
#include <B/Internal/Validate.h>
#include <B/UUID.h>

#include <cstddef>
#include <sstream>
#include <vector>

static B_UUID
b_exception_aggregate_uuid = B_UUID("4B332F25-4DF4-4B19-846E-3610209367FE");

static char const *
b_exception_aggregate_allocate_message(
    struct B_Exception const *);

static void
b_exception_aggregate_deallocate_message(
    char const *);

static void
b_exception_aggregate_deallocate(
    struct B_Exception *);

B_DEFINE_VTABLE_FUNCTION(
    B_ExceptionVTable,
    b_exception_aggregate_vtable,
    {
        .uuid = b_exception_aggregate_uuid,
        .allocate_message = b_exception_aggregate_allocate_message,
        .deallocate_message = b_exception_aggregate_deallocate_message,
        .deallocate = b_exception_aggregate_deallocate,
    })

struct B_ExceptionAggregate {
    B_Exception ex;

    std::vector<B_ExceptionUniquePtr> source_exceptions;

    B_ExceptionAggregate() :
        ex((B_Exception) {
            .vtable = b_exception_aggregate_vtable(),
        }) {
    }
};

static B_ExceptionAggregate *
b_exception_aggregate_validate(
    B_Exception *);

static B_ExceptionAggregate const *
b_exception_aggregate_validate(
    B_Exception const *);

B_Exception *
b_exception_aggregate(
    B_Exception **source_exceptions,
    size_t count) {

    // FIXME(strager): If there's only one exception, should
    // we really make an aggregate wrapper?

    auto ex = new B_ExceptionAggregate();
    // TODO(strager): Move to constructor.
    for (size_t i = 0; i < count; ++i) {
        if (B_Exception *source_ex = source_exceptions[i]) {
            b_exception_validate(source_ex);
            ex->source_exceptions.emplace_back(
                B_ExceptionUniquePtr(source_ex));
        }
    }
    return &ex->ex;
}

static char const *
b_exception_aggregate_allocate_message(
    B_Exception const *ex_raw) {

    B_ExceptionAggregate const *ex
        = b_exception_aggregate_validate(ex_raw);

    std::ostringstream ex_message;
    ex_message << "Aggregate exception:";

    for (B_ExceptionUniquePtr const &source_ex
        : ex->source_exceptions) {

        char const *source_ex_message
            = b_exception_allocate_message(source_ex.get());
        ex_message << "\n - " << source_ex_message;
        b_exception_deallocate_message(
            source_ex.get(),
            source_ex_message);
    }

    return b_strdup(ex_message.str().c_str());
}

static void
b_exception_aggregate_deallocate_message(
    char const *message) {

    free(const_cast<char *>(message));
}

static void
b_exception_aggregate_deallocate(
    B_Exception *ex_raw) {

    B_ExceptionAggregate *ex
        = b_exception_aggregate_validate(ex_raw);
    delete ex;
}

static B_ExceptionAggregate const *
b_exception_aggregate_validate(
    B_Exception const *ex) {

    b_exception_validate(ex);
    B_VALIDATE(b_uuid_equal(
        ex->vtable->uuid,
        b_exception_aggregate_uuid));

    _Static_assert(
        offsetof(B_ExceptionAggregate, ex) == 0,
        "ex must be the first member of struct B_ExceptionAggregate");
    return
        reinterpret_cast<B_ExceptionAggregate const *>(ex);
}

static B_ExceptionAggregate *
b_exception_aggregate_validate(
    B_Exception *ex) {

    return const_cast<B_ExceptionAggregate *>(
        b_exception_aggregate_validate(ex));
}
