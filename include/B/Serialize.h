#ifndef B_HEADER_GUARD_C91313FC_5914_4021_B874_A562BB748EF7
#define B_HEADER_GUARD_C91313FC_5914_4021_B874_A562BB748EF7

#include <B/Base.h>

#include <stddef.h>
#include <stdint.h>

struct B_ErrorHandler;

B_ABSTRACT struct B_ByteSink {
    // Synchronously writes data_size bytes.  If this
    // function returns false, it may be called again.
    B_FUNC
    (*write_bytes)(
            struct B_ByteSink *,
            uint8_t const *data,
            size_t data_size,
            struct B_ErrorHandler const *);

    B_FUNC
    (*deallocate)(
            B_TRANSFER struct B_ByteSink *,
            struct B_ErrorHandler const *);
};

struct B_ByteSinkInMemory {
    struct B_ByteSink super;
    uint8_t *data;
    uint8_t *data_cur;
    uint8_t *data_end;
};

#if defined(__cplusplus)
extern "C" {
#endif

// Creates a ByteSink which serializes into a byte buffer,
// using the given ByteSinkInMemory as storage.
//
// b_byte_sink_in_memory_finalize must be called if this
// function succeeds.
B_EXPORT_FUNC
b_byte_sink_in_memory_initialize(
        B_OUT struct B_ByteSinkInMemory *storage,
        B_BORROWED B_OUTPTR struct B_ByteSink **,
        struct B_ErrorHandler const *);

// Returns the serialized data from a ByteSink created by
// b_byte_sink_in_memory_initialize.
//
// The ByteSink created by b_byte_sink_in_memory_initialize
// is invalidated.
//
// data must be deallocated with b_deallocate.
//
// Subsequent calls to b_byte_sink_in_memory_finalize or
// b_byte_sink_in_memory_release for the same
// ByteSinkInMemory will fail.
B_EXPORT_FUNC
b_byte_sink_in_memory_finalize(
        B_TRANSFER struct B_ByteSinkInMemory *,
        B_TRANSFER B_OUTPTR uint8_t **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *);

// Releases resources allocated by
// b_byte_sink_in_memory_initialize.
//
// The ByteSink created by b_byte_sink_in_memory_initialize
// is invalidated.
//
// Cannot be called if b_byte_sink_in_memory_finalize has
// been called.
//
// Subsequent calls to b_byte_sink_in_memory_finalize or
// b_byte_sink_in_memory_release for the same
// ByteSinkInMemory will fail.
//
// This function is equivalent to a call to
// b_byte_sink_in_memory_finalize followed by a call to
// b_deallocate.
B_EXPORT_FUNC
b_byte_sink_in_memory_release(
        B_TRANSFER struct B_ByteSinkInMemory *,
        struct B_ErrorHandler const *);

// Serializes one byte.
B_EXPORT_FUNC
b_serialize_1(
        struct B_ByteSink *,
        uint8_t,
        struct B_ErrorHandler const *);

// Serializes two bytes in big endian.
B_EXPORT_FUNC
b_serialize_2_be(
        struct B_ByteSink *,
        uint16_t,
        struct B_ErrorHandler const *);

// Serializes four bytes in big endian.
B_EXPORT_FUNC
b_serialize_4_be(
        struct B_ByteSink *,
        uint32_t,
        struct B_ErrorHandler const *);

// Serializes eight bytes in big endian.
B_EXPORT_FUNC
b_serialize_8_be(
        struct B_ByteSink *,
        uint64_t,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_serialize_bytes(
        struct B_ByteSink *,
        uint8_t const *data,
        size_t data_size,
        struct B_ErrorHandler const *);

// Serializes eight bytes in big endian representing the
// size of the data, followed by the data.
B_EXPORT_FUNC
b_serialize_data_and_size_8_be(
        struct B_ByteSink *,
        uint8_t const *data,
        size_t data_size,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <string>

// FIXME(strager): std::string does not use b_allocate.
inline B_FUNC
b_serialize_data_and_size_8_be(
        struct B_ByteSink *sink,
        std::string const &data,
        struct B_ErrorHandler const *eh) {
    return b_serialize_data_and_size_8_be(
        sink,
        reinterpret_cast<uint8_t const *>(data.c_str()),
        data.size(),
        eh);
}
#endif

#endif
