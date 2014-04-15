#include <B/Deserialize.h>
#include <B/Error.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

B_EXPORT_FUNC
b_question_serialize_to_memory(
        struct B_Question const *question,
        struct B_QuestionVTable const *question_vtable,
        B_OUTPTR uint8_t **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, question_vtable);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, data_size);

    struct B_ByteSinkInMemory storage;
    struct B_ByteSink *sink;
    if (!b_byte_sink_in_memory_initialize(
            &storage,
            &sink,
            eh)) {
        return false;
    }
    if (!question_vtable->serialize(question, sink, eh)) {
        (void) b_byte_sink_in_memory_release(&storage, eh);
        return false;
    }
    if (!b_byte_sink_in_memory_finalize(
            &storage,
            data,
            data_size,
            eh)) {
        return false;
    }
    return true;
}

B_EXPORT_FUNC
b_answer_serialize_to_memory(
        struct B_Answer const *answer,
        struct B_AnswerVTable const *answer_vtable,
        B_OUTPTR uint8_t **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, answer);
    B_CHECK_PRECONDITION(eh, answer_vtable);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, data_size);

    struct B_ByteSinkInMemory storage;
    struct B_ByteSink *sink;
    if (!b_byte_sink_in_memory_initialize(
            &storage,
            &sink,
            eh)) {
        return false;
    }
    if (!answer_vtable->serialize(answer, sink, eh)) {
        (void) b_byte_sink_in_memory_release(&storage, eh);
        return false;
    }
    return b_byte_sink_in_memory_finalize(
        &storage,
        data,
        data_size,
        eh);
}

B_EXPORT_FUNC
b_question_deserialize_from_memory(
        struct B_QuestionVTable const *question_vtable,
        uint8_t const *data,
        size_t data_size,
        B_OUTPTR struct B_Question **out_question,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question_vtable);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, out_question);

    struct B_ByteSourceInMemory storage;
    struct B_ByteSource *source;
    if (!b_byte_source_in_memory_initialize(
            &storage,
            data,
            data_size,
            &source,
            eh)) {
        return false;
    }
    return question_vtable->deserialize(
        source,
        out_question,
        eh);
}

B_EXPORT_FUNC
b_answer_deserialize_from_memory(
        struct B_AnswerVTable const *answer_vtable,
        uint8_t const *data,
        size_t data_size,
        B_OUTPTR struct B_Answer **out_answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, answer_vtable);
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, out_answer);

    struct B_ByteSourceInMemory storage;
    struct B_ByteSource *source;
    if (!b_byte_source_in_memory_initialize(
            &storage,
            data,
            data_size,
            &source,
            eh)) {
        return false;
    }
    return answer_vtable->deserialize(
        source,
        out_answer,
        eh);
}
