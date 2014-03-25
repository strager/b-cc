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

#include <cstring>
#include <fstream>

struct FileAnswer :
        public B_AnswerClass<FileAnswer> {
    explicit
    FileAnswer(
            uint64_t sum_hash) :
            sum_hash(sum_hash) {
    }

    static uint64_t
    sum_hash_from_path(
            std::string path) {
        std::ifstream input(path);
        if (!input) {
            return 0;  // FIXME(strager)
        }
        uint64_t sum_hash = 0;
        char c;
        while (input.get(c)) {
            sum_hash += static_cast<uint8_t>(c);
        }
        return sum_hash;
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
        public B_QuestionClass<FileQuestion> {
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

    operator B_FilePath const *() const {
        return static_cast<B_FilePath const *>(
                static_cast<void const *>(this));
    }

    static FileQuestion const *
    cast(
            B_FilePath const *path) {
        return static_cast<FileQuestion const *>(
                static_cast<void const *>(path));
    }

    static FileQuestion *
    cast(
            B_FilePath *path) {
        return static_cast<FileQuestion *>(
                static_cast<void *>(path));
    }

    static B_FUNC
    answer(
            FileQuestion const *self,
            B_OUTPTR FileAnswer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        uint64_t sum_hash = FileAnswer::sum_hash_from_path(
            std::string(*self));
        return b_new(out, eh, sum_hash);
    }

    static B_FUNC
    equal(
            FileQuestion const *a,
            FileQuestion const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return b_file_path_equal(*a, *b, out, eh);
    }

    static B_FUNC
    replicate(
            FileQuestion const *self,
            B_OUTPTR FileQuestion **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        char *string;
        if (!b_strdup(*self, &string, eh)) {
            return false;
        }
        *out = cast(string);
        return true;
    }

    static B_FUNC
    deallocate(
            B_TRANSFER FileQuestion *self,
            B_ErrorHandler const *eh) {
        return b_deallocate(self, eh);
    }

    static B_FUNC
    serialize(
            FileQuestion const *self,
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        size_t length = strlen(*self);
        void *string;
        if (!b_memdup(*self, length, &string, eh)) {
            return false;
        }
        out->data = string;
        out->size = length;
        return true;
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR FileQuestion **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        // TODO(strager): Check for zero bytes and raise.
        char *string;
        if (!b_strndup(
                reinterpret_cast<char *>(serialized.data),
                serialized.size,
                &string,
                eh)) {
            return false;
        }
        *out = cast(string);
        return true;
    }
};

template<>
B_UUID
B_QuestionClass<FileQuestion>::uuid
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
