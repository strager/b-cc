#include <B/Internal/Portable.h>
#include <B/Serialize.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
b_serialize_with_file(
    const char *data,
    size_t data_size,
    void *closure) {
    FILE *stream = (FILE *) closure;
    fwrite(data, 1, data_size, stream);
}

int
b_serialize_to_file(
    FILE *stream,
    const void *value,
    B_SerializeFunc serialize) {
    errno = 0;
    serialize(
        value,
        b_serialize_with_file,
        (void *) stream);
    return errno ? -1 : 0;
}

static size_t
b_deserialize_with_file(
    char *data,
    size_t data_size,
    void *closure) {
    FILE *stream = (FILE *) closure;
    return fread(data, 1, data_size, stream);
}

void *
b_deserialize0(
    void *user_closure,
    B_Deserializer deserializer,
    void *deserializer_closure) {
    return ((B_DeserializeFunc0) user_closure)(
        deserializer,
        deserializer_closure);
}

int
b_deserialize_from_file0(
    FILE *stream,
    void **value,
    B_DeserializeFunc0 deserialize) {
    return b_deserialize_from_file1(
        stream,
        value,
        b_deserialize0,
        (void *) deserialize);
}

int
b_deserialize_from_file1(
    FILE *stream,
    void **value,
    B_DeserializeFunc deserialize,
    void *user_closure) {
    errno = 0;
    *value = deserialize(
        user_closure,
        b_deserialize_with_file,
        (void *) stream);
    return errno ? -1 : 0;
}

int
b_serialize_to_file_path(
    const char *file_path,
    const void *value,
    B_SerializeFunc serialize) {
    FILE *stream = fopen(file_path, "w");
    if (!stream) {
        return -1;
    }
    int err = b_serialize_to_file(
        stream,
        value,
        serialize);
    fclose(stream);
    return err;
}

int
b_deserialize_from_file_path0(
    const char *file_path,
    void **value,
    B_DeserializeFunc0 deserialize) {
    return b_deserialize_from_file_path1(
        file_path,
        value,
        b_deserialize0,
        (void *) deserialize);
}

int
b_deserialize_from_file_path1(
    const char *file_path,
    void **value,
    B_DeserializeFunc deserialize,
    void *user_closure) {
    FILE *stream = fopen(file_path, "r");
    if (!stream) {
        return -1;
    }
    int err = b_deserialize_from_file1(
        stream,
        value,
        deserialize,
        user_closure);
    fclose(stream);
    return err;
}

struct B_SerializeToMemory {
    char *buffer;
    size_t buffer_size;
    size_t cursor;
};

static struct B_SerializeToMemory
b_serialize_to_memory_allocate(void) {
    const size_t buffer_size = 4096;
    return (struct B_SerializeToMemory) {
        .buffer = malloc(buffer_size),
        .buffer_size = buffer_size,
        .cursor = 0,
    };
}

void
b_serialize_to_memory_grow(
    struct B_SerializeToMemory *closure) {
    size_t new_buffer_size = closure->buffer_size * 2;
    assert(new_buffer_size > closure->buffer_size);
    *closure = (struct B_SerializeToMemory) {
        .buffer = b_reallocf(
            closure->buffer,
            new_buffer_size),
        .buffer_size = new_buffer_size,
        .cursor = closure->cursor,
    };
}

void
b_serialize_to_memory_serializer(
    const char *data,
    size_t data_size,
    void *closure_raw) {
    struct B_SerializeToMemory *closure
        = (struct B_SerializeToMemory *) closure_raw;
    while (data_size + closure->cursor > closure->buffer_size) {
        b_serialize_to_memory_grow(closure);
    }
    memcpy(
        closure->buffer + closure->cursor,
        data,
        data_size);
    closure->cursor += data_size;
}

char *
b_serialize_to_memory(
    const void *value,
    B_SerializeFunc serialize,
    size_t *size) {
    struct B_SerializeToMemory closure
        = b_serialize_to_memory_allocate();
    serialize(
        value,
        b_serialize_to_memory_serializer,
        &closure);
    *size = closure.cursor;
    return closure.buffer;
}

struct B_DeserializeFromMemory {
    const char *data;
    size_t data_size;
    size_t cursor;
};

size_t
b_deserialize_from_memory_serializer(
    char *data,
    size_t data_size,
    void *closure_raw) {
    struct B_DeserializeFromMemory *closure
        = (struct B_DeserializeFromMemory *) closure_raw;
    size_t size = b_min_size(
        data_size,
        closure->data_size - closure->cursor);
    memcpy(data, closure->data + closure->cursor, size);
    closure->cursor += size;
    return size;
}

void *
b_deserialize_from_memory0(
    const char *data,
    size_t data_size,
    B_DeserializeFunc0 deserialize) {

    struct B_DeserializeFromMemory closure = {
        .data = data,
        .data_size = data_size,
        .cursor = 0,
    };
    return deserialize(
        b_deserialize_from_memory_serializer,
        &closure);
}

void *
b_deserialize_from_memory1(
    const char *data,
    size_t data_size,
    B_DeserializeFunc deserialize,
    void *user_closure) {
    struct B_DeserializeFromMemory closure = {
        .data = data,
        .data_size = data_size,
        .cursor = 0,
    };
    return deserialize(
        user_closure,
        b_deserialize_from_memory_serializer,
        &closure);
}

void
b_serialize_sized(
    const void *value,
    void (*serialize_size)(size_t, B_Serializer, void *),
    B_SerializeFunc serialize,
    B_Serializer serializer,
    void *serializer_closure) {
    size_t data_size;
    char *data = b_serialize_to_memory(
        value,
        serialize,
        &data_size);
    assert(data);
    serialize_size(
        data_size,
        serializer,
        serializer_closure);
    serializer(
        data,
        data_size,
        serializer_closure);
    free(data);
}

void *
b_deserialize_sized0(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_DeserializeFunc0 deserialize,
    B_Deserializer deserializer,
    void *deserializer_closure) {
    return b_deserialize_sized1(
        deserialize_size,
        b_deserialize0,
        (void *) deserialize,
        deserializer,
        deserializer_closure);
}

void *
b_deserialize_sized1(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_DeserializeFunc deserialize,
    void *user_closure,
    B_Deserializer deserializer,
    void *deserializer_closure) {
    bool ok;
    size_t data_size = deserialize_size(
        &ok,
        deserializer,
        deserializer_closure);
    if (!ok) return NULL;

    char *data = malloc(data_size);
    size_t read_size = deserializer(
        data,
        data_size,
        deserializer_closure);
    if (read_size != data_size) {
        return NULL;
    }

    void *result = b_deserialize_from_memory1(
        data,
        data_size,
        deserialize,
        user_closure);
    free(data);
    return result;
}

// FIXME Handle endianness.  We'd probably want
// little-endian to be faster on the more common
// architectures (ARM-LE, i386, x86_64).
#define B_SERIALIZE_WORD(type, value, serializer, serializer_closure) \
    do { \
        union { \
            type word; \
            char bytes[sizeof(type)]; \
        } u = { .word = (value) }; \
        (serializer)(u.bytes, sizeof(type), (serializer_closure)); \
    } while (0)

#define B_DESERIALIZE_WORD(type, ok, deserializer, deserializer_closure) \
    do { \
        union { \
            type word; \
            char bytes[sizeof(type)]; \
        } u; \
        const size_t size = sizeof(type); \
        size_t read_size = (deserializer)(u.bytes, size, (deserializer_closure)); \
        if (read_size == size) { \
            *(ok) = true; \
            return u.word; \
        } else { \
            *(ok) = false; \
            return 0; \
        } \
    } while (0)

void
b_serialize_uint32(
    uint32_t value,
    B_Serializer s,
    void *c) {
    B_SERIALIZE_WORD(uint32_t, value, s, c);
}

uint32_t
b_deserialize_uint32(
    bool *ok,
    B_Deserializer s,
    void *c) {
    B_DESERIALIZE_WORD(uint32_t, ok, s, c);
}

void
b_serialize_uint64(
    uint64_t value,
    B_Serializer s,
    void *c) {
    B_SERIALIZE_WORD(uint64_t, value, s, c);
}

uint64_t
b_deserialize_uint64(
    bool *ok,
    B_Deserializer s,
    void *c) {
    B_DESERIALIZE_WORD(uint64_t, ok, s, c);
}

void
b_serialize_size_t(
    size_t value,
    B_Serializer s,
    void *c) {
    b_serialize_uint64(value, s, c);
}

size_t
b_deserialize_size_t(
    bool *ok,
    B_Deserializer s,
    void *c) {
    return b_deserialize_uint64(ok, s, c);
}

void *
b_deserialize_blob(
    size_t data_size,
    B_Deserializer deserializer,
    void *deserializer_closure) {
    void *data = malloc(data_size);
    size_t size_read = deserializer(
        data,
        data_size,
        deserializer_closure);
    if (size_read != data_size) {
        free(data);
        return NULL;
    }
    return data;
}

void *
b_deserialize_sized_blob(
    size_t *out_data_size,
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_Deserializer deserializer,
    void *deserializer_closure) {
    bool ok;
    size_t data_size = deserialize_size(
        &ok,
        deserializer,
        deserializer_closure);
    if (!ok) return NULL;

    void *data = b_deserialize_blob(
        data_size,
        deserializer,
        deserializer_closure);
    if (!data) return NULL;

    *out_data_size = data_size;
    return data;
}
