#ifndef B_HEADER_GUARD_E9018E23_2F40_4BB3_938E_B30935357878
#define B_HEADER_GUARD_E9018E23_2F40_4BB3_938E_B30935357878

#include <B/Base.h>
#include <B/Macro.h>

#include <stdint.h>

// Thread-safe: YES
// Signal-safe: NO
struct B_UUID;

struct B_UUID {
    uint8_t data[16];
};

// B_UUID_LITERAL("CE4903AD-73F9-41AE-B56C-578084BC529F")
// has type 'struct B_UUID'.
// TODO(strager): Assert length and dashes.
// FIXME(strager): Bytes aren't ordered properly.
#define B_UUID_LITERAL(x) \
    (B_COMPOUND_INIT_STRUCT(B_UUID, { \
\
        B_UUID_GETHEX8(x, 0), \
        B_UUID_GETHEX8(x, 2), \
        B_UUID_GETHEX8(x, 4), \
        B_UUID_GETHEX8(x, 6), \
\
        B_UUID_GETHEX8(x, 9), \
        B_UUID_GETHEX8(x, 11), \
        B_UUID_GETHEX8(x, 14), \
        B_UUID_GETHEX8(x, 16), \
\
        B_UUID_GETHEX8(x, 19), \
        B_UUID_GETHEX8(x, 21), \
        B_UUID_GETHEX8(x, 24), \
        B_UUID_GETHEX8(x, 26), \
\
        B_UUID_GETHEX8(x, 28), \
        B_UUID_GETHEX8(x, 30), \
        B_UUID_GETHEX8(x, 32), \
        B_UUID_GETHEX8(x, 34), \
    }))

#define B_UUID_GETHEX8(string, offset) \
    ((B_HEX_DIGIT_TO_INT((string)[(offset) + 0]) << 4) \
    + (B_HEX_DIGIT_TO_INT((string)[(offset) + 1]) << 0))

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT bool
b_uuid_equal(
        struct B_UUID a,
        struct B_UUID b);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
inline bool
operator==(
        const B_UUID &a,
        const B_UUID &b) {
    return b_uuid_equal(a, b);
}

inline bool
operator!=(
        const B_UUID &a,
        const B_UUID &b) {
    return !(a == b);
}
#endif

#endif
