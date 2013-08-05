#ifndef PORTABLE_H_E5B130AF_4834_4A83_BC1C_B543D7F8D37D
#define PORTABLE_H_E5B130AF_4834_4A83_BC1C_B543D7F8D37D

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *
b_strdup(
    const char *);

void *
b_reallocf(
    void *,
    size_t);

size_t
b_min_size(
    size_t,
    size_t);

#ifdef __cplusplus
}
#endif

#endif
