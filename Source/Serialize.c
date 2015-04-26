#include <B/Error.h>
#include <B/Memory.h>
#include <B/Private/Assertions.h>
#include <B/Private/Math.h>
#include <B/Private/Memory.h>
#include <B/Serialize.h>

#include <errno.h>
#include <stddef.h>
#include <string.h>

_Static_assert(
  offsetof(struct B_ByteSourceInMemory, super) == 0,
  "B_ByteSourceInMemory::super must be the first member");

static B_FUNC bool
byte_sink_in_memory_write_bytes_(
    struct B_ByteSink *sink,
    uint8_t const *data,
    size_t data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(e);

  struct B_ByteSinkInMemory *storage
    = (struct B_ByteSinkInMemory *) sink;
  B_PRECONDITION(storage);
  B_ASSERT(storage->data <= storage->data_cur);
  B_ASSERT(storage->data_cur <= storage->data_end);

  if ((size_t) (storage->data_end - storage->data_cur)
      < data_size) {
    // Grow the buffer.
    size_t offset = storage->data_cur - storage->data;
    size_t old_size = storage->data_end - storage->data;
    // TODO(strager): Use a smarter algorithm.
    // TODO(strager): Check for overflos.
    size_t new_size = old_size + data_size;

    void *new_data;
    if (storage->data) {
      if (!b_reallocate(
          storage->data,
          new_size,
          &new_data,
          e)) {
        return false;
      }
    } else {
      if (!b_allocate(new_size, &new_data, e)) {
        return false;
      }
    }

    storage->data = new_data;
    storage->data_cur = storage->data + offset;
    storage->data_end = storage->data + new_size;

    B_ASSERT((size_t) (storage->data_end
      - storage->data_cur) >= data_size);
  }

  memcpy(storage->data_cur, data, data_size);
  storage->data_cur += data_size;
  B_ASSERT(storage->data_cur <= storage->data_end);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_byte_sink_in_memory_initialize(
    B_OUT struct B_ByteSinkInMemory *storage,
    B_OUT_BORROW struct B_ByteSink **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(storage);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *storage = (struct B_ByteSinkInMemory) {
    .super = {
      .write_bytes = byte_sink_in_memory_write_bytes_,
      .deallocate = NULL,
    },
    .data = NULL,
    .data_end = NULL,
    .data_cur = NULL,
  };
  *out = &storage->super;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_byte_sink_in_memory_finalize(
    B_TRANSFER struct B_ByteSinkInMemory *storage,
    B_OUT_TRANSFER uint8_t **data,
    B_OUT size_t *data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(storage);
  B_OUT_PARAMETER(data);
  B_OUT_PARAMETER(data_size);
  B_OUT_PARAMETER(e);

  if (storage->data == NULL) {
    B_ASSERT(storage->data_cur == NULL);
    B_ASSERT(storage->data_end == NULL);
    // Give a non-NULL pointer.
    void *empty;
    if (!b_allocate(1, &empty, e)) {
      return false;
    }
    *data = empty;
    *data_size = 0;
    return true;
  }

  *data = storage->data;
  *data_size = storage->data_cur - storage->data;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_byte_sink_in_memory_release(
    B_TRANSFER struct B_ByteSinkInMemory *storage,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(storage);
  B_OUT_PARAMETER(e);

  if (!storage->data) {
    return true;
  }
  b_deallocate(storage->data);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_serialize_1(
    B_BORROW struct B_ByteSink *sink,
    uint8_t x,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  if (!sink->write_bytes(sink, &x, sizeof(x), e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_serialize_2_be(
    B_BORROW struct B_ByteSink *sink,
    uint16_t x,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  uint8_t bytes[sizeof(uint16_t)] = {
    (uint8_t) (x >> 8),
    (uint8_t) (x >> 0),
  };
  if (!sink->write_bytes(sink, bytes, sizeof(bytes), e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_serialize_4_be(
    B_BORROW struct B_ByteSink *sink,
    uint32_t x,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  uint8_t bytes[sizeof(uint32_t)] = {
    (uint8_t) (x >> 24),
    (uint8_t) (x >> 16),
    (uint8_t) (x >> 8),
    (uint8_t) (x >> 0),
  };
  if (!sink->write_bytes(sink, bytes, sizeof(bytes), e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_serialize_8_be(
    B_BORROW struct B_ByteSink *sink,
    uint64_t x,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  uint8_t bytes[sizeof(uint64_t)] = {
    (uint8_t) (x >> 56),
    (uint8_t) (x >> 48),
    (uint8_t) (x >> 40),
    (uint8_t) (x >> 32),
    (uint8_t) (x >> 24),
    (uint8_t) (x >> 16),
    (uint8_t) (x >> 8),
    (uint8_t) (x >> 0),
  };
  if (!sink->write_bytes(sink, bytes, sizeof(bytes), e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_serialize_bytes(
    B_BORROW struct B_ByteSink *sink,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(e);

  if (!sink->write_bytes(sink, data, data_size, e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_serialize_data_and_size_8_be(
    B_BORROW struct B_ByteSink *sink,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sink);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(e);

  if (!b_serialize_8_be(sink, data_size, e)) {
    return false;
  }
  if (!b_serialize_bytes(sink, data, data_size, e)) {
    return false;
  }
  return true;
}

static B_FUNC bool
byte_source_in_memory_read_bytes_(
    B_BORROW struct B_ByteSource *source,
    B_OUT uint8_t *data,
    B_IN_OUT size_t *data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(source);
  B_PRECONDITION(data);
  B_PRECONDITION(data_size);
  B_OUT_PARAMETER(e);

  struct B_ByteSourceInMemory *storage
    = (struct B_ByteSourceInMemory *) source;
  B_PRECONDITION(storage);
  B_ASSERT(storage->data);

  size_t size
    = B_MIN(*data_size, storage->data_size);
  memcpy(data, storage->data, size);
  *data_size = size;
  storage->data += size;
  storage->data_size -= size;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_byte_source_in_memory_initialize(
    B_OUT struct B_ByteSourceInMemory *storage,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT_BORROW struct B_ByteSource **out,
    B_OUT struct B_Error *e) {
  B_OUT_PARAMETER(storage);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *storage = (struct B_ByteSourceInMemory) {
    .super = {
      .read_bytes = byte_source_in_memory_read_bytes_,
      .deallocate = NULL,
    },
    .data = data,
    .data_size = data_size,
  };
  *out = &storage->super;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_deserialize_1(
    B_BORROW struct B_ByteSource *d,
    B_OUT uint8_t *out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(d);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint8_t x;
  size_t size = sizeof(x);
  if (!d->read_bytes(d, &x, &size, e)) {
    return false;
  }
  if (size != sizeof(x)) {
    *e = (struct B_Error) {.posix_error = ENOSPC};
    return false;
  }
  *out = x;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_deserialize_2_be(
    B_BORROW struct B_ByteSource *d,
    B_OUT uint16_t *out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(d);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint8_t bytes[sizeof(uint16_t)];
  size_t size = sizeof(bytes);
  if (!d->read_bytes(d, bytes, &size, e)) {
    return false;
  }
  if (size != sizeof(bytes)) {
    *e = (struct B_Error) {.posix_error = ENOSPC};
    return false;
  }
  *out
    = ((uint16_t) bytes[0] << 8)
    | ((uint16_t) bytes[1] << 0);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_deserialize_4_be(
    B_BORROW struct B_ByteSource *d,
    B_OUT uint32_t *out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(d);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint8_t bytes[sizeof(uint32_t)];
  size_t size = sizeof(bytes);
  if (!d->read_bytes(d, bytes, &size, e)) {
    return false;
  }
  if (size != sizeof(bytes)) {
    *e = (struct B_Error) {.posix_error = ENOSPC};
    return false;
  }
  *out
    = ((uint32_t) bytes[0] << 24)
    | ((uint32_t) bytes[1] << 16)
    | ((uint32_t) bytes[2] << 8)
    | ((uint32_t) bytes[3] << 0);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_deserialize_8_be(
    B_BORROW struct B_ByteSource *d,
    B_OUT uint64_t *out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(d);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint8_t bytes[sizeof(uint64_t)];
  size_t size = sizeof(bytes);
  if (!d->read_bytes(d, bytes, &size, e)) {
    return false;
  }
  if (size != sizeof(bytes)) {
    *e = (struct B_Error) {.posix_error = ENOSPC};
    return false;
  }
  *out
    = ((uint64_t) bytes[0] << 56)
    | ((uint64_t) bytes[1] << 48)
    | ((uint64_t) bytes[2] << 40)
    | ((uint64_t) bytes[3] << 32)
    | ((uint64_t) bytes[4] << 24)
    | ((uint64_t) bytes[5] << 16)
    | ((uint64_t) bytes[6] << 8)
    | ((uint64_t) bytes[7] << 0);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_deserialize_bytes(
    B_BORROW struct B_ByteSource *source,
    size_t data_size,
    B_OUT uint8_t *data,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(source);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(e);

  size_t read_size = data_size;
  if (!source->read_bytes(source, data, &read_size, e)) {
    return false;
  }
  if (read_size != data_size) {
    *e = (struct B_Error) {.posix_error = ENOSPC};
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_deserialize_data_and_size_8_be(
    B_BORROW struct B_ByteSource *d,
    B_OUT_TRANSFER uint8_t **data,
    B_OUT size_t *data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(d);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(data_size);
  B_OUT_PARAMETER(e);

  uint64_t size_64;
  if (!b_deserialize_8_be(d, &size_64, e)) {
    return false;
  }
  if (size_64 > SIZE_MAX) {
    *e = (struct B_Error) {.posix_error = ENOMEM};
    return false;
  }
  size_t size = (size_t) size_64;

  uint8_t *buffer;
  if (!b_allocate(size, (void **) &buffer, e)) {
    return false;
  }
  size_t read_size = size;
  if (!d->read_bytes(d, buffer, &read_size, e)) {
    goto fail;
  }
  if (read_size != size) {
    goto fail;
  }

  *data = buffer;
  *data_size = size;
  return true;

fail:
  b_deallocate(buffer);
  return false;
}
