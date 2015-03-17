#ifndef B_HEADER_GUARD_F49AB188_F50C_428E_8F34_2DD311AD8AE5
#define B_HEADER_GUARD_F49AB188_F50C_428E_8F34_2DD311AD8AE5

#include <B/Base.h>
#include <B/Config.h>

struct B_ErrorHandler;

#define B_MIN(x, y) ((x) < (y) ? (x) : (y))
#define B_MAX(x, y) ((x) > (y) ? (x) : (y))

#if defined(__cplusplus)
extern "C" {
#endif

// Duplicates args such that a call to b_deallocate on the
// output pointer will deallocate the entire args array,
// including strings.
B_EXPORT_FUNC
b_dup_args(
        char const *const *args,
        B_OUTPTR char const *const **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
