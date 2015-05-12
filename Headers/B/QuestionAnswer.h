#pragma once

#include <B/Attributes.h>
#include <B/UUID.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct B_ByteSink;
struct B_ByteSource;
struct B_Error;

struct B_IQuestion;

struct B_IAnswer;

struct B_AnswerVTable {
  B_WUR B_FUNC void (*deallocate)(
      B_TRANSFER struct B_IAnswer *);

  B_WUR B_FUNC bool (*replicate)(
      B_BORROW struct B_IAnswer const *,
      B_OUT_TRANSFER struct B_IAnswer **,
      B_OUT struct B_Error *);

  B_WUR B_FUNC bool (*serialize)(
      B_BORROW struct B_IAnswer const *,
      B_BORROW struct B_ByteSink *,
      B_OUT struct B_Error *);

  B_WUR B_FUNC bool (*deserialize)(
      B_BORROW struct B_ByteSource *,
      B_OUT_TRANSFER struct B_IAnswer **,
      B_OUT struct B_Error *);

  B_WUR B_FUNC bool (*equal)(
      B_BORROW struct B_IAnswer const *,
      B_BORROW struct B_IAnswer const *);
};

struct B_QuestionVTable {
  struct B_UUID uuid;
  struct B_AnswerVTable const *answer_vtable;

  B_WUR B_FUNC bool (*query_answer)(
      B_BORROW struct B_IQuestion const *,
      B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **,
      B_OUT struct B_Error *);

  B_WUR B_FUNC void (*deallocate)(
      B_TRANSFER struct B_IQuestion *);

  B_WUR B_FUNC bool (*replicate)(
      B_BORROW struct B_IQuestion const *,
      B_OUT_TRANSFER struct B_IQuestion **,
      B_OUT struct B_Error *);

  B_WUR B_FUNC bool (*serialize)(
      B_BORROW struct B_IQuestion const *,
      B_BORROW struct B_ByteSink *,
      B_OUT struct B_Error *);

  B_WUR B_FUNC bool (*deserialize)(
      B_BORROW struct B_ByteSource *,
      B_OUT_TRANSFER struct B_IQuestion **,
      B_OUT struct B_Error *);
};

B_WUR B_EXPORT_FUNC bool
b_question_serialize_to_memory(
    B_BORROW struct B_IQuestion const *,
    B_BORROW struct B_QuestionVTable const *,
    B_OUT_TRANSFER uint8_t **data,
    B_OUT size_t *data_size,
    struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_question_deserialize_from_memory(
    B_BORROW struct B_QuestionVTable const *,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT_TRANSFER struct B_IQuestion **,
    struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_serialize_to_memory(
    B_BORROW struct B_IAnswer const *,
    B_BORROW struct B_AnswerVTable const *,
    B_OUT_TRANSFER uint8_t **data,
    B_OUT size_t *data_size,
    struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_deserialize_from_memory(
    B_BORROW struct B_AnswerVTable const *,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT_TRANSFER struct B_IAnswer **,
    struct B_Error *);
