#include "UUID.h"
#include "Validate.h"

#include <stdlib.h>
#include <string.h>

struct B_UUID
b_uuid_from_stable_string(
    const char *s) {
    return ((struct B_UUID) { .uuid = s });
}

struct B_UUID
b_uuid_from_temp_string(
    const char *s) {
    size_t size = strlen(s) + 1;
    char *uuid = malloc(size);
    memcpy(uuid, s, size);
    return b_uuid_from_stable_string(uuid);
}

bool
b_uuid_equal(
    struct B_UUID a,
    struct B_UUID b) {
    return strcmp(a.uuid, b.uuid) == 0;
}

void
b_uuid_validate(struct B_UUID uuid) {
    B_VALIDATE(uuid.uuid);
}
