#include <B/Alloc.h>
#include <B/Error.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

B_EXPORT_FUNC
b_allocate(
        size_t byte_count,
        B_OUTPTR void **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    void *p = malloc(byte_count);
    if (!p) {
        // TODO(strager): Error.
        abort();
    }

    *out = p;
    return true;
}

B_EXPORT_FUNC
b_deallocate(
        B_TRANSFER void *p,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, p);

    free(p);

    return true;
}
