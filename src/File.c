#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Deserialize.h>
#include <B/Error.h>
#include <B/File.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct FileAnswer_ {
    // TODO(strager): Store a hash instead (e.g. SHA-256).
    uint64_t B_CONST_STRUCT_MEMBER sum_hash;
};

static B_FUNC
sum_hash_from_file_(
        FILE *file,
        uint64_t *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, file);
    B_CHECK_PRECONDITION(eh, out);
    uint64_t sum_hash = 0;
    for (;;) {
        char buffer[1024];
        size_t read = fread(
            buffer, 1, sizeof(buffer), file);
        for (size_t i = 0; i < read; ++i) {
            sum_hash += (uint8_t) buffer[i];
        }
        if (read != sizeof(buffer)) {
            break;
        }
    }
    if (!feof(file)) {
        // TODO(strager): Retry, etc.
        (void) B_RAISE_ERRNO_ERROR(eh, errno, "fread");
        return false;
    }
    *out = sum_hash;
    return true;
}

static B_FUNC
file_answer_from_sum_hash_(
        uint64_t sum_hash,
        B_OUTPTR struct B_Answer **out,
        struct B_ErrorHandler const *eh) {
    struct FileAnswer_ *answer;
    if (!b_allocate(
            sizeof(*answer), (void **) &answer, eh)) {
        return false;
    }
    *answer = (struct FileAnswer_) {
        .sum_hash = sum_hash,
    };
    *out = (void *) answer;
    return true;
}

static B_FilePath const *
file_question_path_(
        struct B_Question const *question) {
    return (void const *) question;
}

static B_FUNC
file_question_query_answer_(
        struct B_Question const *question,
        B_OUTPTR struct B_Answer **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, out);
    B_FilePath const *path = file_question_path_(question);

retry_open:;
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno == ENOENT) {
            *out = NULL;
            return true;
        }
        switch (B_RAISE_ERRNO_ERROR(
                eh, errno, "fopen")) {
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        case B_ERROR_RETRY:
            goto retry_open;
        }
    }

    uint64_t sum_hash;
    bool ok = sum_hash_from_file_(file, &sum_hash, eh);
    (void) fclose(file);  // FIXME(strager)
    if (!ok) {
        return false;
    }

    return file_answer_from_sum_hash_(sum_hash, out, eh);
}

static B_FUNC
file_question_equal_(
        struct B_Question const *a,
        struct B_Question const *b,
        B_OUT bool *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, a);
    B_CHECK_PRECONDITION(eh, b);
    B_CHECK_PRECONDITION(eh, out);
    return b_file_path_equal(
        file_question_path_(a),
        file_question_path_(b),
        out,
        eh);
}

static B_FUNC
file_question_replicate_(
        struct B_Question const *question,
        B_OUTPTR struct B_Question **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, out);

    B_FilePath const *path = file_question_path_(question);
    char *string;
    if (!b_strdup(path, &string, eh)) {
        return false;
    }
    *out = (struct B_Question *) string;
    return true;
}

static B_FUNC
file_question_deallocate_(
        B_TRANSFER struct B_Question *question,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question);
    return b_deallocate(question, eh);
}

static B_FUNC
file_question_serialize_(
        struct B_Question const *question,
        struct B_ByteSink *sink,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, sink);

    B_FilePath const *path = file_question_path_(question);
    return b_serialize_data_and_size_8_be(
        sink, (uint8_t const *) path, strlen(path), eh);
}

static B_FUNC
file_question_deserialize_(
        struct B_ByteSource *source,
        B_OUTPTR struct B_Question **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, source);
    B_CHECK_PRECONDITION(eh, out);

    uint8_t *data;
    size_t data_size;
    if (!b_deserialize_data_and_size_8_be(
            source, &data, &data_size, eh)) {
        return false;
    }

    char *path;
    if (!b_strndup(
            (char const *) data, data_size, &path, eh)) {
        (void) b_deallocate(data, eh);
        return false;
    }

    (void) b_deallocate(data, eh);

    *out = (struct B_Question *) path;
    return true;
}

static struct FileAnswer_ const *
file_answer_(
        struct B_Answer const *answer) {
    return (void const *) answer;
}

static B_FUNC
file_answer_equal_(
        struct B_Answer const *a,
        struct B_Answer const *b,
        B_OUTPTR bool *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, a);
    B_CHECK_PRECONDITION(eh, b);
    B_CHECK_PRECONDITION(eh, out);
    *out = file_answer_(a)->sum_hash
        == file_answer_(b)->sum_hash;
    return true;
}

static B_FUNC
file_answer_replicate_(
        struct B_Answer const *answer,
        B_OUTPTR struct B_Answer **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, answer);
    B_CHECK_PRECONDITION(eh, out);
    return file_answer_from_sum_hash_(
        file_answer_(answer)->sum_hash, out, eh);
}

static B_FUNC
file_answer_deallocate_(
        struct B_Answer *answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, answer);
    return b_deallocate(answer, eh);
}

static B_FUNC
file_answer_serialize_(
        struct B_Answer const *answer,
        struct B_ByteSink *sink,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, answer);
    B_CHECK_PRECONDITION(eh, sink);
    return b_serialize_8_be(
        sink, file_answer_(answer)->sum_hash, eh);
}

static B_FUNC
file_answer_deserialize_(
        struct B_ByteSource *source,
        B_OUTPTR struct B_Answer **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, source);
    B_CHECK_PRECONDITION(eh, out);
    uint64_t sum_hash;
    if (!b_deserialize_8_be(source, &sum_hash, eh)) {
        return false;
    }
    // TODO(strager): Check for end-of-file.
    return file_answer_from_sum_hash_(sum_hash, out, eh);
}

static struct B_AnswerVTable const
file_answer_vtable_ = {
    .equal = file_answer_equal_,
    .replicate = file_answer_replicate_,
    .deallocate = file_answer_deallocate_,
    .serialize = file_answer_serialize_,
    .deserialize = file_answer_deserialize_,
};

static struct B_QuestionVTable const
file_question_vtable_ = {
    .uuid = B_UUID_INITIALIZER(
        B6BD5D3B, DDC1, 43B2, 832B, 2B5836BF78FC),
    .answer_vtable = &file_answer_vtable_,
    .query_answer = file_question_query_answer_,
    .equal = file_question_equal_,
    .replicate = file_question_replicate_,
    .deallocate = file_question_deallocate_,
    .serialize = file_question_serialize_,
    .deserialize = file_question_deserialize_,
};

struct B_QuestionVTable const *
b_file_contents_question_vtable(
        void) {
    return &file_question_vtable_;
}

struct B_QuestionVTable const *
b_directory_listing_question_vtable(
        void) {
    B_NYI();
}

B_EXPORT_FUNC
b_directory_listing_answer_strings(
        struct B_Answer const *answer,
        B_OUTPTR B_FilePath const **path,
        struct B_ErrorHandler const *eh) {
    (void) answer;
    (void) path;
    (void) eh;
    B_NYI();
}
