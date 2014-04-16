// GCC 4.8.2 has a false warning.  With -Werror, this fails
// the build.  The following block changes the failure into
// a mere warning.
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Deserialize.h>
#include <B/File.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

#include <cstdio>
#include <cstring>
#include <errno.h>

struct FileAnswer :
        public B_AnswerClass<FileAnswer> {
    explicit
    FileAnswer(
            uint64_t sum_hash) :
            sum_hash(sum_hash) {
    }

    static B_FUNC
    sum_hash_from_path(
            std::string path,
            uint64_t *out,
            B_ErrorHandler const *eh) {
retry_open:;
        FILE *file = fopen(path.c_str(), "r");
        if (!file) {
            switch (B_RAISE_ERRNO_ERROR(
                    eh,
                    errno,
                    "fopen")) {
            case B_ERROR_ABORT:
            case B_ERROR_IGNORE:
                return false;
            case B_ERROR_RETRY:
                goto retry_open;
            }
        }
        bool ok = sum_hash_from_file(file, out, eh);
        (void) fclose(file);
        return ok;
    }

    static B_FUNC
    sum_hash_from_file(
            FILE *file,
            uint64_t *out,
            B_ErrorHandler const *eh) {
        uint64_t sum_hash = 0;
        int c;
        while ((c = fgetc(file)) != -1) {
            sum_hash += static_cast<uint8_t>(c);
        }
        if (!feof(file)) {
            (void) B_RAISE_ERRNO_ERROR(eh, errno, "fgetc");
            return false;
        }
        *out = sum_hash;
        return true;
    }

    static B_FUNC
    equal(
            FileAnswer const *a,
            FileAnswer const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = *a == *b;
        return true;
    }

    static B_FUNC
    replicate(
            FileAnswer const *self,
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = new FileAnswer(*self);
        return true;
    }

    static B_FUNC
    deallocate(
            FileAnswer const *self,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        delete self;
        return true;
    }

    static B_FUNC
    serialize(
            FileAnswer const *self,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, sink);
        return b_serialize_8_be(sink, self->sum_hash, eh);
    }

    static B_FUNC
    deserialize(
            B_ByteSource *source,
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, source);
        B_CHECK_PRECONDITION(eh, out);

        uint64_t sum_hash;
        if (!b_deserialize_8_be(source, &sum_hash, eh)) {
            return false;
        }
        // TODO(strager): Check for end-of-file.

        *out = new FileAnswer(sum_hash);
        return true;
    }

    bool
    operator==(
            FileAnswer const &other) const {
        return other.sum_hash == this->sum_hash;
    }

    // TODO(strager): Store a hash instead (e.g. SHA-256).
    uint64_t const sum_hash;
};

struct FileQuestion :
        public B_QuestionClass<FileQuestion, B_FilePath> {
    typedef FileAnswer AnswerClass;

    FileQuestion() = delete;
    FileQuestion(
            FileQuestion const &) = delete;
    FileQuestion(
            FileQuestion &&) = delete;
    FileQuestion &
    operator=(
            FileQuestion const &) = delete;
    FileQuestion &
    operator=(
            FileQuestion &&) = delete;

    static B_FUNC
    answer(
            B_FilePath const *path,
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        uint64_t sum_hash;
        if (!FileAnswer::sum_hash_from_path(
                std::string(path),
                &sum_hash,
                eh)) {
            return false;
        }
        return b_new(out, eh, sum_hash);
    }

    static B_FUNC
    equal(
            B_FilePath const *a,
            B_FilePath const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return b_file_path_equal(a, b, out, eh);
    }

    static B_FUNC
    replicate(
            B_FilePath const *path,
            B_OUTPTR B_FilePath **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        char *string;
        if (!b_strdup(path, &string, eh)) {
            return false;
        }
        *out = string;
        return true;
    }

    static B_FUNC
    deallocate(
            B_TRANSFER B_FilePath *path,
            B_ErrorHandler const *eh) {
        return b_deallocate(path, eh);
    }

    static B_FUNC
    serialize(
            B_FilePath const *path,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, sink);

        return b_serialize_data_and_size_8_be(
            sink,
            reinterpret_cast<uint8_t const *>(path),
            strlen(path),
            eh);
    }

    static B_FUNC
    deserialize(
            B_ByteSource *source,
            B_OUTPTR B_FilePath **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, source);
        B_CHECK_PRECONDITION(eh, out);

        uint8_t *data;
        size_t data_size;
        if (!b_deserialize_data_and_size_8_be(
                source,
                &data,
                &data_size,
                eh)) {
            return false;
        }

        char *path;
        if (!b_strndup(
                reinterpret_cast<char const *>(data),
                data_size,
                &path,
                eh)) {
            (void) b_deallocate(data, eh);
            return false;
        }

        (void) b_deallocate(data, eh);

        *out = path;
        return true;
    }
};

template<>
B_UUID
B_QuestionClass<FileQuestion, B_FilePath>::uuid
    = B_UUID_LITERAL("B6BD5D3B-DDC1-43B2-832B-2B5836BF78FC");

struct B_QuestionVTable const *
b_file_contents_question_vtable(
        void) {
    return FileQuestion::vtable();
}

struct B_QuestionVTable const *
b_directory_listing_question_vtable(
        void) {
    B_NYI();
}

B_EXPORT_FUNC
b_directory_listing_answer_strings(
        struct B_Answer const *,
        B_OUTPTR B_FilePath const **,
        struct B_ErrorHandler const *) {
    B_NYI();
}
