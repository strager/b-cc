#pragma once

#include <B/Attributes.h>

#include <stddef.h>
#include <stdint.h>

struct B_Error;

struct B_ByteSink {
  // Synchronously writes data_size bytes.  If this
  // function returns false, it may be called again.
  B_FUNC bool (*write_bytes)(
      B_BORROW struct B_ByteSink *,
      B_BORROW uint8_t const *data,
      size_t data_size,
      B_OUT struct B_Error *);

  B_FUNC bool (*deallocate)(
      B_TRANSFER struct B_ByteSink *,
      B_OUT struct B_Error *);
};

struct B_ByteSinkInMemory {
  struct B_ByteSink super;
  uint8_t *data;
  uint8_t *data_cur;
  uint8_t *data_end;
};

struct B_ByteSource {
  // Synchronously reads at most *data_size bytes. If
  // *data_size after the call does not match *data_size
  // before the call, end-of-stream was encountered.
  B_FUNC bool (*read_bytes)(
      B_BORROW struct B_ByteSource *,
      B_OUT uint8_t *data,
      B_IN_OUT size_t *data_size,
      B_OUT struct B_Error *);

  B_FUNC bool (*deallocate)(
      B_TRANSFER struct B_ByteSource *,
      B_OUT struct B_Error *);
};

struct B_ByteSourceInMemory {
    struct B_ByteSource super;
    uint8_t const *data;
    size_t data_size;
};

#if defined(__cplusplus)
extern "C" {
#endif

// Creates a ByteSink which serializes into a byte buffer,
// using the given ByteSinkInMemory as storage.
//
// b_byte_sink_in_memory_finalize must be called if this
// function succeeds.
B_WUR B_EXPORT_FUNC bool
b_byte_sink_in_memory_initialize(
    B_OUT struct B_ByteSinkInMemory *storage,
    B_OUT_BORROW struct B_ByteSink **,
    B_OUT struct B_Error *);

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
B_WUR B_EXPORT_FUNC bool
b_byte_sink_in_memory_finalize(
    B_TRANSFER struct B_ByteSinkInMemory *,
    B_OUT_TRANSFER uint8_t **data,
    B_OUT size_t *data_size,
    B_OUT struct B_Error *);

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
B_WUR B_EXPORT_FUNC bool
b_byte_sink_in_memory_release(
    B_TRANSFER struct B_ByteSinkInMemory *,
    B_OUT struct B_Error *);

// Serializes one byte.
B_WUR B_EXPORT_FUNC bool
b_serialize_1(
    B_BORROW struct B_ByteSink *,
    uint8_t,
    B_OUT struct B_Error *);

// Serializes two bytes in big endian.
B_WUR B_EXPORT_FUNC bool
b_serialize_2_be(
    B_BORROW struct B_ByteSink *,
    uint16_t,
    B_OUT struct B_Error *);

// Serializes four bytes in big endian.
B_WUR B_EXPORT_FUNC bool
b_serialize_4_be(
    B_BORROW struct B_ByteSink *,
    uint32_t,
    B_OUT struct B_Error *);

// Serializes eight bytes in big endian.
B_WUR B_EXPORT_FUNC bool
b_serialize_8_be(
    B_BORROW struct B_ByteSink *,
    uint64_t,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_serialize_bytes(
    B_BORROW struct B_ByteSink *,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT struct B_Error *);

// Serializes eight bytes in big endian representing the
// size of the data, followed by the data.
B_WUR B_EXPORT_FUNC bool
b_serialize_data_and_size_8_be(
    B_BORROW struct B_ByteSink *,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT struct B_Error *);

// Creates a ByteSource from a byte buffer, using the given
// ByteSourceBytes as storage.
B_WUR B_EXPORT_FUNC bool
b_byte_source_in_memory_initialize(
    B_OUT struct B_ByteSourceInMemory *storage,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT_BORROW struct B_ByteSource **,
    B_OUT struct B_Error *);

// Deserializes the first byte of the ByteSource and returns
// it as a big endian integer. If the byte is not
// available, raises ENOSPC and returns false.
B_WUR B_EXPORT_FUNC bool
b_deserialize_1(
    B_BORROW struct B_ByteSource *,
    B_OUT uint8_t *,
    B_OUT struct B_Error *);

// Deserializes the first two bytes of the ByteSource and
// returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_WUR B_EXPORT_FUNC bool
b_deserialize_2_be(
    B_BORROW struct B_ByteSource *,
    B_OUT uint16_t *,
    B_OUT struct B_Error *);

// Deserializes the first four bytes of the ByteSource and
// returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_WUR B_EXPORT_FUNC bool
b_deserialize_4_be(
    B_BORROW struct B_ByteSource *,
    B_OUT uint32_t *,
    B_OUT struct B_Error *);

// Deserializes the first eight bytes of the ByteSource and
// returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_WUR B_EXPORT_FUNC bool
b_deserialize_8_be(
    B_BORROW struct B_ByteSource *,
    B_OUT uint64_t *,
    B_OUT struct B_Error *);

// Deserializes the first few bytes of the ByteSource. If
// bytes are not available, raises ENOSPC and returns false.
B_WUR B_EXPORT_FUNC bool
b_deserialize_bytes(
    B_BORROW struct B_ByteSource *,
    size_t data_size,
    B_OUT uint8_t *data,
    B_OUT struct B_Error *);

// Deserializes the first eight bytes of the ByteSource
// as a big endian integer, then deserializes that many
// following bytes. If bytes are not available, raises
// ENOSPC and returns false. data must be deallocated with
// b_deallocate.
B_WUR B_EXPORT_FUNC bool
b_deserialize_data_and_size_8_be(
    B_BORROW struct B_ByteSource *,
    B_OUT_TRANSFER uint8_t **data,
    B_OUT size_t *data_size,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
