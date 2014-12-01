#include <B/UUID.h>

#include <string.h>

B_EXPORT bool
b_uuid_equal(
        struct B_UUID const *a,
        struct B_UUID const *b) {
    return memcmp(a, b, sizeof(struct B_UUID)) == 0;
}
