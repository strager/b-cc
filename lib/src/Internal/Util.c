#include <B/Exception.h>
#include <B/Internal/Portable.h>
#include <B/Internal/Util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

static char
b_hexdump_ascii(
    uint8_t c) {

    // TODO(strager): Force locale.
    if (c < 0x80 && isprint(c)) {
        return c;
    } else {
        return '.';
    }
}

struct B_Exception *
b_hexdump(
    void const *data,
    size_t data_size,
    size_t bytes_per_line,
    struct B_Exception *(*line_callback)(
        char const *line,
        void *user_closure),
    void *user_closure) {

    const size_t line_length
        = b_hexdump_line_length(bytes_per_line);
    const size_t line_size = line_length + 1;

    char line[line_size];
    for (size_t i = 0; i < data_size; i += bytes_per_line) {
        struct B_Exception *ex;

        ex = b_hexdump_line(
            line,
            line_size,
            data + i,
            b_min_size(data_size - i, bytes_per_line),
            (uint32_t) i,
            bytes_per_line);
        if (ex) {
            return ex;
        }

        ex = line_callback(line, user_closure);
        if (ex) {
            return ex;
        }
    }

    return NULL;
}

struct B_Exception *
b_hexdump_line(
    char *const buffer,
    size_t const buffer_size,
    void const *const data,
    size_t const data_size,
    uint32_t const base_address,
    size_t const bytes_per_line) {

    if (data_size > bytes_per_line) {
        return b_exception_string("Data size too large");
    }

    size_t expected_size
        = b_hexdump_line_length(bytes_per_line) + 1;
    if (buffer_size < expected_size) {
        return b_exception_string("Buffer too small");
    }

    size_t remaining_size = expected_size;
    char *buffer_ptr = buffer;
    uint8_t const *data_ptr = data;

#define B_APPEND(...) \
    do { \
        int rc = snprintf( \
            buffer_ptr, \
            remaining_size, \
            __VA_ARGS__); \
        if (rc == -1) { \
            return b_exception_errno("snprintf", errno); \
        } \
        assert(rc >= 0); \
        remaining_size -= rc; \
        buffer_ptr += rc; \
    } while (0)

    B_APPEND("%08x  ", base_address);
    for (size_t i = 0; i < data_size; ++i) {
        B_APPEND("%02x ", data_ptr[i]);
        if (i % 8 == 7) {
            B_APPEND(" ");
        }
    }
    for (size_t i = data_size; i < bytes_per_line; ++i) {
        B_APPEND("   ");
        if (i % 8 == 7) {
            B_APPEND(" ");
        }
    }

    B_APPEND("|");
    for (size_t i = 0; i < data_size; ++i) {
        B_APPEND("%c", b_hexdump_ascii(data_ptr[i]));
    }
    B_APPEND("|");

#undef B_APPEND

    //assert(remaining_size == bytes_per_line - data_size);

    return NULL;
}

size_t
b_hexdump_line_length(
    size_t bytes_per_line) {

    // Each line uses twelve characters (8 hex, 2 spaces, 2
    // pipes).  Each byte uses four characters (1 ASCII, 2
    // hex, 1 space).  Every eight bytes uses one character
    // (1 space).
    return 12 + (bytes_per_line * 4) + (bytes_per_line / 8);
}

uint32_t
b_hash_add_8(
    uint32_t hash,
    uint8_t value) {

    // sdbm; public domain.
    // http://www.cse.yorku.ca/~oz/hash.html#sdbm
    return
        value
        + (hash << 6)
        + (hash << 16)
        - hash;
}

uint32_t
b_hash_add_32(
    uint32_t hash,
    uint32_t value) {

    hash = b_hash_add_8(hash, value >> 0);
    hash = b_hash_add_8(hash, value >> 8);
    hash = b_hash_add_8(hash, value >> 16);
    hash = b_hash_add_8(hash, value >> 24);
    return hash;
}

uint32_t
b_hash_add_64(
    uint32_t hash,
    uint64_t value) {

    hash = b_hash_add_32(hash, value >> 0);
    hash = b_hash_add_32(hash, value >> 32);
    return hash;
}

uint32_t
b_hash_add_pointer(
    uint32_t hash,
    void const *value) {

    if (sizeof(value) == 4) {
        return b_hash_add_32(hash, (uintptr_t) value);
    } else if (sizeof(value) == 8) {
        return b_hash_add_64(hash, (uintptr_t) value);
    } else {
        _Static_assert(
            sizeof(value) == 4 || sizeof(value) == 8,
            "Non-{4,8}-byte pointers not supported.");
    }
}
