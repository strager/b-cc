#ifndef UTIL_H_7447E8FE_9310_4BE3_AA3E_1411FFDC10FC
#define UTIL_H_7447E8FE_9310_4BE3_AA3E_1411FFDC10FC

#include <B/Internal/Common.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_Exception;

B_ERRFUNC
b_hexdump(
    void const *data,
    size_t data_size,
    size_t bytes_per_line,
    struct B_Exception *(*line_callback)(
        char const *line,
        void *user_closure),
    void *user_closure);

// Format mimics hexdump -C:
//
// 00000400  20 68 61 73 6b 65 6c 6c  0a 0a 24 28 4c 49 42 29  | haskell..$(LIB)|
// 00000410  3a 20 24 28 4c 49 42 5f  4f 5f 46 49 4c 45 53 29  |: $(LIB_O_FILES)|
//
// Does not include trailing newline.
B_ERRFUNC
b_hexdump_line(
    char *buffer,
    size_t buffer_size,
    void const *data,
    size_t data_size,
    uint32_t base_address,
    size_t bytes_per_line);

size_t
b_hexdump_line_length(
    size_t bytes_per_line);

uint32_t
b_hash_add_8(
    uint32_t hash,
    uint8_t value);

uint32_t
b_hash_add_32(
    uint32_t hash,
    uint32_t value);

uint32_t
b_hash_add_64(
    uint32_t hash,
    uint64_t value);

uint32_t
b_hash_add_pointer(
    uint32_t hash,
    void const *value);

#ifdef __cplusplus
}
#endif

#endif
