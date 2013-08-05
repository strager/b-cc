#include "Portable.h"
#include "Serialize.h"

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

int
b_deserialize_from_file(
    FILE *stream,
    void **value,
    B_DeserializeFunc deserialize) {
    errno = 0;
    *value = deserialize(
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
    int err = b_serialize_to_file(stream, value, serialize);
    fclose(stream);
    return err;
}

int
b_deserialize_from_file_path(
    const char *file_path,
    void **value,
    B_DeserializeFunc deserialize) {
    FILE *stream = fopen(file_path, "r");
    if (!stream) {
        return -1;
    }
    int err = b_deserialize_from_file(stream, value, deserialize);
    fclose(stream);
    return err;
}

struct B_SerializeToMemory {
    char *buffer;
    size_t buffer_size;
    size_t cursor;
};

static struct B_SerializeToMemory
b_serialize_to_memory_allocate() {
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
    return size;
}

void *
b_deserialize_from_memory(
    const char *data,
    size_t data_size,
    B_DeserializeFunc deserialize) {
    struct B_DeserializeFromMemory closure = {
        .data = data,
        .data_size = data_size,
        .cursor = 0,
    };
    return deserialize(
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
b_deserialize_sized(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_DeserializeFunc deserialize,
    B_Deserializer deserializer,
    void *deserializer_closure) {
    bool ok;
    size_t data_size = deserialize_size(
        &ok,
        deserializer,
        deserializer_closure);
    if (!ok) return NULL;

    char *data = malloc(data_size);
    void *result = b_deserialize_from_memory(
        data,
        data_size,
        deserialize);
    free(data);
    return result;
}

// FIXME Handle endianness.  We'd probably want
// little-endian to be faster on the more common
// architectures (ARM-LE, i386, x86_64).
#define B_SERIALIZE_WORD(type, value, serializer, serializer_closure) \
    do { \
        const size_t size = sizeof(type); \
        union { \
            type word; \
            char bytes[size]; \
        } u = { .word = (value) }; \
        (serializer)(u.bytes, size, (serializer_closure)); \
    } while (0)

#define B_DESERIALIZE_WORD(type, ok, deserializer, deserializer_closure) \
    do { \
        const size_t size = sizeof(type); \
        union { \
            type word; \
            char bytes[size]; \
        } u; \
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
