#ifndef SERIALIZE_H_AD6288A4_D5F0_425D_89DE_2A145C1BC44C
#define SERIALIZE_H_AD6288A4_D5F0_425D_89DE_2A145C1BC44C

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*B_Serializer)(
    const char *data,
    size_t data_size,
    void *closure);

typedef size_t (*B_Deserializer)(
    char *data,
    size_t data_size,
    void *closure);

typedef void (*B_SerializeFunc)(
    const void *value,
    B_Serializer,
    void *serializer_closure);

typedef void *(*B_DeserializeFunc)(
    B_Deserializer,
    void *deserializer_closure);

int
b_serialize_to_file(
    FILE *stream,
    const void *value,
    B_SerializeFunc);

int
b_deserialize_from_file(
    FILE *stream,
    void **value,
    B_DeserializeFunc);

int
b_serialize_to_file_path(
    const char *file_path,
    const void *value,
    B_SerializeFunc);

int
b_deserialize_from_file_path(
    const char *file_path,
    void **value,
    B_DeserializeFunc);

char *
b_serialize_to_memory(
    const void *value,
    B_SerializeFunc,
    size_t *size);

void *
b_deserialize_from_memory(
    const char *data,
    size_t data_size,
    B_DeserializeFunc);

void
b_serialize_sized(
    const void *value,
    void (*serialize_size)(size_t, B_Serializer, void *),
    B_SerializeFunc,
    B_Serializer,
    void *serializer_closure);

void *
b_deserialize_sized(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_DeserializeFunc,
    B_Deserializer,
    void *deserializer_closure);

void
b_serialize_uint32(
    uint32_t,
    B_Serializer,
    void *serializer_closure);

uint32_t
b_deserialize_uint32(
    bool *ok,
    B_Deserializer,
    void *deserializer_closure);

void
b_serialize_uint64(
    uint64_t,
    B_Serializer,
    void *serializer_closure);

uint64_t
b_deserialize_uint64(
    bool *ok,
    B_Deserializer,
    void *deserializer_closure);

// Serializes a 64-bit integer.
void
b_serialize_size_t(
    size_t,
    B_Serializer,
    void *serializer_closure);

// Deserializes a 64-bit integer.
size_t
b_deserialize_size_t(
    bool *ok,
    B_Deserializer,
    void *deserializer_closure);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <memory>

template<typename T>
void
b_serialize(
    const T &value,
    B_Serializer serializer,
    void *serializer_closure) {
    value.serialize(serializer, serializer_closure);
}

template<typename T>
void
b_serialize(
    const void *value,
    B_Serializer serializer,
    void *serializer_closure) {
    b_serialize(
        *static_cast<const T *>(value),
        serializer,
        serializer_closure);
}

template<typename T>
std::unique_ptr<T>
b_deserialize(
    B_Deserializer deserializer,
    void *deserializer_closure) {
    return T::deserialize(deserializer, deserializer_closure);
}

template<typename T>
void
b_serialize_sized(
    const T &value,
    void (*serialize_size)(size_t, B_Serializer, void *),
    B_Serializer serializer,
    void *serializer_closure) {
    b_serialize_sized(
        &value,
        serialize_size,
        [](const void *v, B_Serializer s, void *c) -> void {
            b_serialize(*static_cast<const T *>(v), s, c);
        },
        serializer,
        serializer_closure
    );
}

template<typename T>
std::unique_ptr<T>
b_deserialize_sized(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_Deserializer deserializer,
    void *deserializer_closure) {
    void *value = b_deserialize_sized(
        deserialize_size,
        [](B_Deserializer s, void *c) -> void * {
            auto value = b_deserialize<T>(s, c);
            return static_cast<void *>(value.release());
        },
        deserializer,
        deserializer_closure
    );
    return std::unique_ptr<T>(static_cast<T *>(value));
}
#endif

#endif
