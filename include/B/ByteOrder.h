#ifndef B_HEADER_GUARD_9C03E288_C58F_4354_9A92_F7FF4E4B4056
#define B_HEADER_GUARD_9C03E288_C58F_4354_9A92_F7FF4E4B4056

#include <B/Config.h>

#if defined(B_CONFIG_BSWAP_BUILTIN)
# define B_16_SWAP(x) (__builtin_bswap16(x))
# define B_32_SWAP(x) (__builtin_bswap32(x))
# define B_64_SWAP(x) (__builtin_bswap64(x))
#elif defined(B_CONFIG_LIBKERN)
# include <libkern/OSByteOrder.h>
# define B_16_SWAP(x) (OSSwapInt16(x))
# define B_32_SWAP(x) (OSSwapInt32(x))
# define B_64_SWAP(x) (OSSwapInt64(x))
#else
// FIXME(strager): Should we provide a sane default?
# error Unknown byte swap implementation
#endif

// B_##_TO_BE converts an N-bit host integer to its
// big-endian form.

// B_##_FROM_BE converts an N-bit big-endian integer to its
// host form.

#if defined(B_CONFIG_LITTLE_ENDIAN)
# define B_16_TO_BE(x) (B_16_SWAP(x))
# define B_32_TO_BE(x) (B_32_SWAP(x))
# define B_64_TO_BE(x) (B_64_SWAP(x))
# define B_16_FROM_BE(x) (B_16_SWAP(x))
# define B_32_FROM_BE(x) (B_32_SWAP(x))
# define B_64_FROM_BE(x) (B_64_SWAP(x))
#elif defined(B_CONFIG_BIG_ENDIAN)
# define B_16_TO_BE(x) (x)
# define B_32_TO_BE(x) (x)
# define B_64_TO_BE(x) (x)
# define B_16_FROM_BE(x) (x)
# define B_32_FROM_BE(x) (x)
# define B_64_FROM_BE(x) (x)
#else
# error Unknown endian
#endif

#endif
