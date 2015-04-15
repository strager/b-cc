#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

// FIXME(strager): Callers currently assume the output is
// NULLed or untouched on failure.
B_WUR B_EXPORT_FUNC bool
b_allocate(
    size_t,
    B_OUT_TRANSFER void **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_allocate2(
    size_t,
    size_t,
    B_OUT_TRANSFER void **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_reallocate(
    B_TRANSFER void *,
    size_t,
    B_OUT_TRANSFER void **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_deallocate(
    B_TRANSFER void *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_strdup(
    B_BORROW char const *,
    B_OUT_TRANSFER char **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_memdup(
    B_BORROW void const *,
    size_t,
    B_OUT_TRANSFER void **,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
