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

typedef void *(*B_DeserializeFunc0)(
    B_Deserializer,
    void *deserializer_closure);

typedef void *(*B_DeserializeFunc)(
    void *user_closure,
    B_Deserializer,
    void *deserializer_closure);

int
b_serialize_to_file(
    FILE *stream,
    const void *value,
    B_SerializeFunc);

int
b_deserialize_from_file0(
    FILE *stream,
    void **value,
    B_DeserializeFunc0);

int
b_deserialize_from_file1(
    FILE *stream,
    void **value,
    B_DeserializeFunc,
    void *user_closure);

int
b_serialize_to_file_path(
    const char *file_path,
    const void *value,
    B_SerializeFunc);

int
b_deserialize_from_file_path0(
    const char *file_path,
    void **value,
    B_DeserializeFunc0);

int
b_deserialize_from_file_path1(
    const char *file_path,
    void **value,
    B_DeserializeFunc,
    void *user_closure);

char *
b_serialize_to_memory(
    const void *value,
    B_SerializeFunc,
    size_t *size);

void *
b_deserialize_from_memory0(
    const char *data,
    size_t data_size,
    B_DeserializeFunc0);

void *
b_deserialize_from_memory1(
    const char *data,
    size_t data_size,
    B_DeserializeFunc,
    void *user_closure);

void
b_serialize_sized(
    const void *value,
    void (*serialize_size)(size_t, B_Serializer, void *),
    B_SerializeFunc,
    B_Serializer,
    void *serializer_closure);

void *
b_deserialize_sized0(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_DeserializeFunc0,
    B_Deserializer,
    void *deserializer_closure);

void *
b_deserialize_sized1(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_DeserializeFunc,
    void *user_closure,
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

void *
b_deserialize_blob(
    size_t data_size,
    B_Deserializer,
    void *deserializer_closure);

void *
b_deserialize_sized_blob(
    size_t *data_size,
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    B_Deserializer,
    void *deserializer_closure);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <cassert>
#include <memory>
#include <type_traits>

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

#define B_RESULT_OF_DESERIALIZE_FUNC \
    typename std::result_of<TDeserializeFunc(B_Deserializer, void *)>::type
template<typename TDeserializeFunc>
B_RESULT_OF_DESERIALIZE_FUNC
b_deserialize_sized(
    size_t (*deserialize_size)(bool *ok, B_Deserializer, void *),
    TDeserializeFunc deserialize,
    B_Deserializer deserializer,
    void *deserializer_closure) {
    typedef B_RESULT_OF_DESERIALIZE_FUNC result_type;
    struct Payload {
        Payload(
            TDeserializeFunc &deserialize) :
            deserialize(deserialize) {
        }

        TDeserializeFunc &deserialize;
        // FIXME 'result' should not be default-constructed.
        result_type result;
    };

    Payload payload(deserialize);
    void *returned_payload = b_deserialize_sized1(
        deserialize_size,
        [](
            void *payload_raw,
            B_Deserializer s,
            void *c) -> void * {
            Payload &payload
                = *static_cast<Payload *>(payload_raw);
            payload.result = payload.deserialize(s, c);
            return &payload;
        },
        &payload,
        deserializer,
        deserializer_closure
    );
    assert(returned_payload);
    return std::move(payload.result);
}
#undef B_RESULT_OF_DESERIALIZE_FUNC
#endif

#endif
