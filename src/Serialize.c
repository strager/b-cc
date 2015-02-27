#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Error.h>
#include <B/Serialize.h>

#include <string.h>

static B_FUNC
byte_sink_in_memory_write_bytes_(
        struct B_ByteSink *sink,
        uint8_t const *data,
        size_t data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
    B_CHECK_PRECONDITION(eh, data);

    struct B_ByteSinkInMemory *storage
        = (struct B_ByteSinkInMemory *) sink;
    B_CHECK_PRECONDITION(eh, storage);
    B_ASSERT(storage->data <= storage->data_cur);
    B_ASSERT(storage->data_cur <= storage->data_end);

    if ((size_t) (storage->data_end - storage->data_cur)
            < data_size) {
        // Grow the buffer.
        size_t offset = storage->data_cur - storage->data;
        size_t old_size = storage->data_end - storage->data;
        // TODO(strager): Use a smarter algorithm.
        size_t new_size = old_size + data_size;

        void *new_data;
        if (storage->data) {
            if (!b_reallocate(
                    new_size,
                    storage->data,
                    &new_data,
                    eh)) {
                return false;
            }
        } else {
            if (!b_allocate(new_size, &new_data, eh)) {
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

B_EXPORT_FUNC
b_byte_sink_in_memory_initialize(
        B_OUT struct B_ByteSinkInMemory *storage,
        B_BORROWED B_OUTPTR struct B_ByteSink **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, storage);
    B_CHECK_PRECONDITION(eh, out);

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

B_EXPORT_FUNC
b_byte_sink_in_memory_finalize(
        B_TRANSFER struct B_ByteSinkInMemory *storage,
        B_TRANSFER B_OUTPTR uint8_t **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, storage);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, data_size);

    if (storage->data == NULL) {
        B_ASSERT(storage->data_cur == NULL);
        B_ASSERT(storage->data_end == NULL);
        // Give a non-NULL pointer.
        void *empty;
        if (!b_allocate(0, &empty, eh)) {
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

B_EXPORT_FUNC
b_byte_sink_in_memory_release(
        B_TRANSFER struct B_ByteSinkInMemory *storage,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, storage);

    if (!storage->data) {
        return true;
    }
    return b_deallocate(storage->data, eh);
}

B_EXPORT_FUNC
b_serialize_1(
        struct B_ByteSink *sink,
        uint8_t x,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
    return sink->write_bytes(sink, &x, sizeof(x), eh);
}

B_EXPORT_FUNC
b_serialize_2_be(
        struct B_ByteSink *sink,
        uint16_t x,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
    uint8_t bytes[sizeof(uint16_t)] = {
        (uint8_t) (x >> 8),
        (uint8_t) (x >> 0),
    };
    return sink->write_bytes(
        sink, bytes, sizeof(bytes), eh);
}

B_EXPORT_FUNC
b_serialize_4_be(
        struct B_ByteSink *sink,
        uint32_t x,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
    uint8_t bytes[sizeof(uint32_t)] = {
        (uint8_t) (x >> 24),
        (uint8_t) (x >> 16),
        (uint8_t) (x >> 8),
        (uint8_t) (x >> 0),
    };
    return sink->write_bytes(
        sink, bytes, sizeof(bytes), eh);
}

B_EXPORT_FUNC
b_serialize_8_be(
        struct B_ByteSink *sink,
        uint64_t x,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
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
    return sink->write_bytes(
        sink, bytes, sizeof(bytes), eh);
}

B_EXPORT_FUNC
b_serialize_bytes(
        struct B_ByteSink *sink,
        uint8_t const *data,
        size_t data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
    B_CHECK_PRECONDITION(eh, data);
    return sink->write_bytes(sink, data, data_size, eh);
}

B_EXPORT_FUNC
b_serialize_data_and_size_8_be(
        struct B_ByteSink *sink,
        uint8_t const *data,
        size_t data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sink);
    B_CHECK_PRECONDITION(eh, data);
    if (!b_serialize_8_be(sink, data_size, eh)) {
        return false;
    }
    return b_serialize_bytes(sink, data, data_size, eh);
}
