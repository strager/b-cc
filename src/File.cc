// GCC 4.8.2 has a false warning.  With -Werror, this fails
// the build.  The following block changes the failure into
// a mere warning.
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/File.h>
#include <B/QuestionAnswer.h>

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
        FILE *f = fopen(path.c_str(), "r");
        if (!f) {
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

        uint64_t sum_hash = 0;
        int c;
        while ((c = fgetc(f)) != -1) {
            sum_hash += static_cast<uint8_t>(c);
        }
        if (!feof(f)) {
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
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        size_t size = sizeof(uint64_t);
        if (!b_allocate(size, &out->data, eh)) {
            return false;
        }
        // FIXME(strager): This is endianness-dependent.
        *reinterpret_cast<uint64_t *>(out->data)
            = self->sum_hash;
        out->size = size;
        return true;
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);
        B_CHECK_PRECONDITION(eh, serialized.data);

        if (serialized.size != sizeof(uint64_t)) {
            // TODO(strager): Report error.
            return false;
        }
        // FIXME(strager): This is endianness-dependent.
        *out = new FileAnswer(*reinterpret_cast<uint64_t *>(
            serialized.data));
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
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        size_t length = strlen(path);
        size_t size = sizeof(uint64_t) + length;
        void *data;
        if (!b_allocate(size, &data, eh)) {
            return false;
        }
        // FIXME(strager): This is endianness-dependent.
        *reinterpret_cast<uint64_t *>(data) = length;
        memcpy(
            reinterpret_cast<uint8_t *>(data)
                + sizeof(uint64_t),
            path,
            length);

        out->data = data;
        out->size = size;
        return true;
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR B_FilePath **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        if (serialized.size < sizeof(uint64_t)) {
            B_RAISE_ERRNO_ERROR(eh, ENOSPC, "deserialize");
            return false;
        }

        // FIXME(strager): This is endianness-dependent.
        uint64_t length_raw = *reinterpret_cast<uint64_t *>(
            serialized.data);
        if (length_raw > SIZE_MAX) {
            B_RAISE_ERRNO_ERROR(
                eh,
                EOVERFLOW,
                "deserialize");
            return false;
        }
        size_t length = (size_t) length_raw;
        if (length > serialized.size + sizeof(uint64_t)) {
            B_RAISE_ERRNO_ERROR(eh, ENOSPC, "deserialize");
        }

        // TODO(strager): Check for zero bytes and raise.
        char *path;
        if (!b_strndup(
                reinterpret_cast<char *>(serialized.data)
                    + sizeof(uint64_t),
                length,
                &path,
                eh)) {
            return false;
        }

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
