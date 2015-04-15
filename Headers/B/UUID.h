#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stdint.h>

// Thread-safe: YES
// Signal-safe: NO
struct B_UUID;

struct B_UUID {
    uint8_t data[16];
};

// B_UUID_INITIALIZER(
//     CE4903AD, 73F9, 41AE, B56C, 578084BC529F)
// is an initializer for B_UUID.
// TODO(strager): Assert length?
// FIXME(strager): Bytes aren't ordered properly.
#define B_UUID_INITIALIZER(a, b, c, d, e) \
  B_UUID_INITIALIZER_( \
    0x##a##LL, \
    0x##b##LL, \
    0x##c##LL, \
    0x##d##LL, \
    0x##e##LL)

#define B_UUID_INITIALIZER_(a, b, c, d, e) \
  {{ \
    (a >> 24) & 0xFF, \
    (a >> 16) & 0xFF, \
    (a >>  8) & 0xFF, \
    (a >>  0) & 0xFF, \
\
    (b >>  8) & 0xFF, \
    (b >>  0) & 0xFF, \
\
    (c >>  8) & 0xFF, \
    (c >>  0) & 0xFF, \
\
    (d >>  8) & 0xFF, \
    (d >>  0) & 0xFF, \
\
    (e >> 40) & 0xFF, \
    (e >> 32) & 0xFF, \
    (e >> 24) & 0xFF, \
    (e >> 16) & 0xFF, \
    (e >>  8) & 0xFF, \
    (e >>  0) & 0xFF, \
  }}

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_uuid_equal(
        B_BORROW struct B_UUID const *,
        B_BORROW struct B_UUID const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
inline bool
operator==(
    const B_UUID &a,
    const B_UUID &b) {
  return b_uuid_equal(&a, &b);
}

inline bool
operator!=(
    const B_UUID &a,
    const B_UUID &b) {
  return !(a == b);
}
#endif
