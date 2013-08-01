#include "Portable.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char *
b_strdup(const char *original) {
    size_t size = strlen(original) + 1;
    char *s = malloc(size);
    memcpy(s, original, size);
    return s;
}
