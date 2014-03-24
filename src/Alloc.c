#include <B/Alloc.h>
#include <B/Error.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

B_EXPORT_FUNC
b_memdup(
        void const *data,
        size_t byte_count,
        B_OUTPTR void **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, out);

    void *p;
    if (!b_allocate(byte_count, &p, eh)) {
        return false;
    }
    memcpy(p, data, byte_count);

    *out = p;
    return true;
}

B_EXPORT_FUNC
b_strdup(
        char const *string,
        B_OUTPTR char **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, string);
    B_CHECK_PRECONDITION(eh, out);
    return b_memdup(
        string,
        strlen(string) + 1,
        (void **) out,
        eh);
}

B_EXPORT_FUNC
b_strndup(
        char const *string,
        size_t string_length,
        B_OUTPTR char **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, string);
    B_CHECK_PRECONDITION(eh, out);

    void *p;
    if (!b_allocate(string_length + 1, &p, eh)) {
        return false;
    }
    strncpy(p, string, string_length);
    ((char *) p)[string_length] = '\0';

    *out = p;
    return true;
}
