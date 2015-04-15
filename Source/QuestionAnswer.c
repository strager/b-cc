#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/Private/Log.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

B_WUR B_EXPORT_FUNC bool
b_question_serialize_to_memory(
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_QuestionVTable const *vtable,
    B_OUT_TRANSFER uint8_t **out_data,
    B_OUT size_t *out_data_size,
    struct B_Error *e) {
  B_PRECONDITION(question);
  B_PRECONDITION(vtable);
  B_OUT_PARAMETER(out_data);
  B_OUT_PARAMETER(out_data_size);
  B_OUT_PARAMETER(e);

  struct B_ByteSinkInMemory storage;
  struct B_ByteSink *sink;
  if (!b_byte_sink_in_memory_initialize(
      &storage, &sink, e)) {
    return false;
  }
  if (!vtable->serialize(question, sink, e)) {
    (void) b_byte_sink_in_memory_release(
      &storage, &(struct B_Error) {});
    return false;
  }
  uint8_t *data;
  size_t data_size;
  if (!b_byte_sink_in_memory_finalize(
      &storage, &data, &data_size, e)) {
    return false;
  }
  *out_data = data;
  *out_data_size = data_size;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_question_deserialize_from_memory(
    B_BORROW struct B_QuestionVTable const *vtable,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT_TRANSFER struct B_IQuestion **out,
    struct B_Error *e) {
  B_PRECONDITION(vtable);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_ByteSourceInMemory storage;
  struct B_ByteSource *source;
  if (!b_byte_source_in_memory_initialize(
      &storage, data, data_size, &source, e)) {
    return false;
  }
  struct B_IQuestion *question;
  if (!vtable->deserialize(source, &question, e)) {
    return false;
  }
  *out = question;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_serialize_to_memory(
    B_BORROW struct B_IAnswer const *answer,
    B_BORROW struct B_AnswerVTable const *vtable,
    B_OUT_TRANSFER uint8_t **out_data,
    B_OUT size_t *out_data_size,
    struct B_Error *e) {
  B_PRECONDITION(answer);
  B_PRECONDITION(vtable);
  B_OUT_PARAMETER(out_data);
  B_OUT_PARAMETER(out_data_size);
  B_OUT_PARAMETER(e);

  struct B_ByteSinkInMemory storage;
  struct B_ByteSink *sink;
  if (!b_byte_sink_in_memory_initialize(
      &storage, &sink, e)) {
    return false;
  }
  if (!vtable->serialize(answer, sink, e)) {
    (void) b_byte_sink_in_memory_release(
      &storage, &(struct B_Error) {});
    return false;
  }
  uint8_t *data;
  size_t data_size;
  if (!b_byte_sink_in_memory_finalize(
      &storage, &data, &data_size, e)) {
    return false;
  }
  *out_data = data;
  *out_data_size = data_size;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_deserialize_from_memory(
    B_BORROW struct B_AnswerVTable const *vtable,
    B_BORROW uint8_t const *data,
    size_t data_size,
    B_OUT_TRANSFER struct B_IAnswer **out,
    struct B_Error *e) {
  B_PRECONDITION(vtable);
  B_PRECONDITION(data);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_ByteSourceInMemory storage;
  struct B_ByteSource *source;
  if (!b_byte_source_in_memory_initialize(
      &storage, data, data_size, &source, e)) {
    return false;
  }
  struct B_IAnswer *answer;
  if (!vtable->deserialize(source, &answer, e)) {
    return false;
  }
  *out = answer;
  return true;
}
