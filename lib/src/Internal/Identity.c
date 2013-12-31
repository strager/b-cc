#include <B/Internal/Allocate.h>
#include <B/Internal/Identity.h>
#include <B/Internal/Portable.h>

#include <stdlib.h>
#include <string.h>

struct B_Identity *
b_identity_allocate(
    char const *data,
    size_t data_size) {

    char *data_copy = b_memdup(data, data_size);
    if (!data_copy) {
        return NULL;
    }

    B_ALLOCATE_RETURN(struct B_Identity, {
        .data = data_copy,
        .data_size = data_size,
    });
}

void
b_identity_deallocate(
    struct B_Identity *identity) {

    free((char *) identity->data);
    free(identity);
}

bool
b_identity_equal(
    struct B_Identity const *x,
    struct B_Identity const *y) {

    return x->data_size == y->data_size
        && memcmp(x->data, y->data, x->data_size) == 0;
}
