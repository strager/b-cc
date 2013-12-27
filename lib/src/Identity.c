#include <B/Allocate.h>
#include <B/Identity.h>
#include <B/Portable.h>

#include <stdlib.h>

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
