#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/ByteOrder.h>
#include <B/Deserialize.h>
#include <B/Error.h>
#include <B/Misc.h>

#include <errno.h>
#include <string.h>

B_STATIC_ASSERT(
    offsetof(struct B_ByteSourceInMemory, super) == 0,
    "B_ByteSourceInMemory::super must be the first member");

static B_FUNC
byte_source_in_memory_read_bytes_(
        struct B_ByteSource *source,
        uint8_t *data,
        size_t *data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, data_size);

    struct B_ByteSourceInMemory *storage
        = (struct B_ByteSourceInMemory *) source;
    B_CHECK_PRECONDITION(eh, storage);
    B_ASSERT(storage->data);

    size_t size
        = B_MIN(*data_size, storage->data_size);
    memcpy(data, storage->data, size);
    *data_size = size;
    storage->data += size;
    storage->data_size -= size;
    return true;
}

B_EXPORT_FUNC
b_byte_source_in_memory_initialize(
        B_OUT struct B_ByteSourceInMemory *storage,
        uint8_t const *data,
        size_t data_size,
        B_BORROWED B_OUTPTR struct B_ByteSource **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, storage);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, out);

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

B_EXPORT_FUNC
b_deserialize_1(
        struct B_ByteSource *d,
        B_OUT uint8_t *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, d);
    B_CHECK_PRECONDITION(eh, out);
    uint8_t x;
    size_t size = sizeof(x);
    if (!d->read_bytes(d, &x, &size, eh)) {
        return false;
    }
    if (size != sizeof(x)) {
        B_RAISE_ERRNO_ERROR(eh, ENOSPC, "b_deserialize_1");
        return false;
    }
    *out = x;
    return true;
}

// Deserializes the first two bytes of the ByteSource
// and returns it as a big endian integer.  If bytes are not
// available, raises ENOSPC and returns false.
B_EXPORT_FUNC
b_deserialize_2_be(
        struct B_ByteSource *d,
        B_OUT uint16_t *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, d);
    B_CHECK_PRECONDITION(eh, out);
    uint16_t x;
    size_t size = sizeof(x);
    if (!d->read_bytes(d, (uint8_t *) &x, &size, eh)) {
        return false;
    }
    if (size != sizeof(x)) {
        B_RAISE_ERRNO_ERROR(
            eh,
            ENOSPC,
            "b_deserialize_2_be");
        return false;
    }
    *out = B_16_FROM_BE(x);
    return true;
}

B_EXPORT_FUNC
b_deserialize_4_be(
        struct B_ByteSource *d,
        B_OUT uint32_t *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, d);
    B_CHECK_PRECONDITION(eh, out);
    uint32_t x;
    size_t size = sizeof(x);
    if (!d->read_bytes(d, (uint8_t *) &x, &size, eh)) {
        return false;
    }
    if (size != sizeof(x)) {
        B_RAISE_ERRNO_ERROR(
            eh,
            ENOSPC,
            "b_deserialize_4_be");
        return false;
    }
    *out = B_32_FROM_BE(x);
    return true;
}

B_EXPORT_FUNC
b_deserialize_8_be(
        struct B_ByteSource *d,
        B_OUT uint64_t *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, d);
    B_CHECK_PRECONDITION(eh, out);
    uint64_t x;
    size_t size = sizeof(x);
    if (!d->read_bytes(d, (uint8_t *) &x, &size, eh)) {
        return false;
    }
    if (size != sizeof(x)) {
        B_RAISE_ERRNO_ERROR(
            eh,
            ENOSPC,
            "b_deserialize_8_be");
        return false;
    }
    *out = B_64_FROM_BE(x);
    return true;
}

B_EXPORT_FUNC
b_deserialize_data_and_size_8_be(
        struct B_ByteSource *d,
        B_OUTPTR uint8_t **data,
        size_t *data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, d);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, data_size);
    uint64_t size_64;
    if (!b_deserialize_8_be(d, &size_64, eh)) {
        return false;
    }
    if (size_64 > SIZE_MAX) {
        B_RAISE_ERRNO_ERROR(
            eh,
            ENOMEM,
            "b_deserialize_data_and_size_8_be");
        return false;
    }
    size_t size = (size_t) size_64;

    uint8_t *buffer;
    if (!b_allocate(size, (void **) &buffer, eh)) {
        return false;
    }
    size_t read_size = size;
    if (!d->read_bytes(d, buffer, &read_size, eh)) {
        goto fail;
    }
    if (read_size != size) {
        goto fail;
    }

    *data = buffer;
    *data_size = size;
    return true;

fail:
    (void) b_deallocate(buffer, eh);
    return false;
}
