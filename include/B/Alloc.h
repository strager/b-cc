#ifndef B_HEADER_GUARD_74620237_3A50_4F49_87C0_3ECA94843444
#define B_HEADER_GUARD_74620237_3A50_4F49_87C0_3ECA94843444

#include <B/Base.h>
#include <B/Config.h>

#include <stddef.h>

struct B_ErrorHandler;

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_allocate(
        size_t byte_count,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_deallocate(
        B_TRANSFER void *,
        struct B_ErrorHandler const *);

// Ownership is *not* transferred if this function fails.
// This matches the semantics of realloc, not reallocf.
B_EXPORT_FUNC
b_reallocate(
        size_t new_byte_count,
        B_TRANSFER void *,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_memdup(
        void const *data,
        size_t byte_count,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_memdupplus(
        void const *data,
        size_t byte_count,
        size_t extra_size,
        B_OUTPTR void **,
        struct B_ErrorHandler const *);

// Allocates memory and copies from string until a 0
// terminator is found.  Resulting data is 0-terminated.
B_EXPORT_FUNC
b_strdup(
        char const *string,
        B_OUTPTR char **,
        struct B_ErrorHandler const *);

// Like b_strdup, but reserves some number of bytes after
// the 0 terminator.
B_EXPORT_FUNC
b_strdupplus(
        char const *string,
        size_t extra_size,
        B_OUTPTR char **,
        struct B_ErrorHandler const *);

// Allocates memory and copies from string until either a 0
// terminator is found or string_length bytes have been
// copied.  Resulting data is 0-terminated.
B_EXPORT_FUNC
b_strndup(
        char const *string,
        size_t string_length,
        B_OUTPTR char **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
