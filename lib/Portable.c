#include "Portable.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char *
b_strdup(
    const char *original) {
    size_t size = strlen(original) + 1;
    char *s = malloc(size);
    memcpy(s, original, size);
    return s;
}

void *
b_reallocf(
    void *buffer,
    size_t new_size) {
    void *new_buffer = realloc(buffer, new_size);
    if (!new_buffer) {
        free(buffer);
    }
    return new_buffer;
}

size_t
b_min_size(
    size_t a,
    size_t b) {
    return a < b ? a : b;
}
