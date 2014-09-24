#ifndef B_HEADER_GUARD_678A12C7_7876_4B25_9C84_B99B240256E1
#define B_HEADER_GUARD_678A12C7_7876_4B25_9C84_B99B240256E1

#include <B/Base.h>

#include <stddef.h>
#include <stdint.h>

struct B_ErrorHandler;

B_ABSTRACT struct B_ByteSource {
    // Synchronously reads at most *data_size bytes.  If
    // *data_size after the call does not match *data_size
    // before the call, end-of-stream was encountered.
    B_FUNC
    (*read_bytes)(
            struct B_ByteSource *,
            uint8_t *data,
            size_t *data_size,
            struct B_ErrorHandler const *);

    B_FUNC
    (*deallocate)(
            B_TRANSFER struct B_ByteSource *,
            struct B_ErrorHandler const *);
};

struct B_ByteSourceInMemory {
    struct B_ByteSource super;
    uint8_t const *data;
    size_t data_size;
};

#if defined(__cplusplus)
extern "C" {
#endif

// Creates a ByteSource from a byte buffer, using the given
// ByteSourceBytes as storage.
B_EXPORT_FUNC
b_byte_source_in_memory_initialize(
        B_OUT struct B_ByteSourceInMemory *storage,
        uint8_t const *data,
        size_t data_size,
        B_BORROWED B_OUTPTR struct B_ByteSource **,
        struct B_ErrorHandler const *);

// Deserializes the first byte of the ByteSource and returns
// it as a big endian integer.  If the byte is not
// available, raises ENOSPC and returns false.
B_EXPORT_FUNC
b_deserialize_1(
        struct B_ByteSource *,
        B_OUT uint8_t *,
        struct B_ErrorHandler const *);

// Deserializes the first two bytes of the ByteSource and
// returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_EXPORT_FUNC
b_deserialize_2_be(
        struct B_ByteSource *,
        B_OUT uint16_t *,
        struct B_ErrorHandler const *);

// Deserializes the first four bytes of the ByteSource and
// returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_EXPORT_FUNC
b_deserialize_4_be(
        struct B_ByteSource *,
        B_OUT uint32_t *,
        struct B_ErrorHandler const *);

// Deserializes the first eight bytes of the ByteSource and
// returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_EXPORT_FUNC
b_deserialize_8_be(
        struct B_ByteSource *,
        B_OUT uint64_t *,
        struct B_ErrorHandler const *);

// Deserializes the first eight bytes of the Deserializeable
// as a big endian integer, then deserializes that many
// following bytes.  If bytes are not available, raises
// ENOSPC and returns false.  data must be deallocated with
// b_deallocate.
B_EXPORT_FUNC
b_deserialize_data_and_size_8_be(
        struct B_ByteSource *,
        B_OUTPTR uint8_t **data,
        size_t *data_size,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Alloc.h>

# include <string>

// FIXME(strager): std::string does not use b_allocate.
inline B_FUNC
b_deserialize_data_and_size_8_be(
        struct B_ByteSource *source,
        std::string *data,
        struct B_ErrorHandler const *eh) {
    uint8_t *raw_data;
    size_t data_size;
    if (!b_deserialize_data_and_size_8_be(
            source, &raw_data, &data_size, eh)) {
        return false;
    }
    data->assign(
        reinterpret_cast<char *>(raw_data), data_size);
    if (!b_deallocate(raw_data, eh)) {
        return false;
    }
    return true;
}
#endif

#endif
