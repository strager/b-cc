#include "Allocate.h"
#include "Portable.h"
#include "UUID.h"
#include "Validate.h"

#include <stdlib.h>
#include <string.h>

// TODO Make thread-safe.
// TODO Use Mach malloc zones if available.
struct B_UUIDAllocationList {
    void *allocation;
    struct B_UUIDAllocationList *next;
};

static struct B_UUIDAllocationList *
uuid_allocations = NULL;

static void
b_uuid_record_allocation(
    void *allocation) {
    B_ALLOCATE(struct B_UUIDAllocationList, node, {
        .allocation = allocation,
        .next = uuid_allocations,
    });
    uuid_allocations = node;
}

static void
b_uuid_free_allocation_list(
    struct B_UUIDAllocationList *list) {
    if (list) {
        b_uuid_free_allocation_list(list->next);
        free(list->allocation);
        free(list);
    }
}

struct B_UUID
b_uuid_from_stable_string(
    const char *s) {
    return ((struct B_UUID) { .uuid = s });
}

struct B_UUID
b_uuid_from_temp_string(
    const char *uuid) {
    char *stable = b_strdup(uuid);
    b_uuid_record_allocation(stable);
    return b_uuid_from_stable_string(stable);
}

void
b_uuid_deallocate_leaked(void) {
    b_uuid_free_allocation_list(uuid_allocations);
}

bool
b_uuid_equal(
    struct B_UUID a,
    struct B_UUID b) {
    return b_uuid_compare(a, b) == 0;
}

int
b_uuid_compare(
    struct B_UUID a,
    struct B_UUID b) {
    return strcmp(a.uuid, b.uuid);
}

void
b_uuid_validate(struct B_UUID uuid) {
    B_VALIDATE(uuid.uuid);
}
