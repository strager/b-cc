#ifndef B_HEADER_GUARD_DB5E9C00_57E8_472F_9D69_9C7AB86C2F1E
#define B_HEADER_GUARD_DB5E9C00_57E8_472F_9D69_9C7AB86C2F1E

#include <B/Base.h>
#include <B/UUID.h>

#include <stdbool.h>
#include <stddef.h>

struct B_AnswerContext;
struct B_ByteSink;
struct B_ByteSource;
struct B_ErrorHandler;

// A Question is value describing a question on the current
// state of the system.  For example, "is this file
// present?", "what is the content of this file?", and "what
// is this environmental variable set to?" are valid
// questions.
//
// Dependencies in B are from one Question to another.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_Question;

struct B_QuestionVTable;

// An Answer is a value describing the state of part of the
// system.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_Answer;

struct B_AnswerVTable;

// A function to be called when an answer is available for a
// previously-asked question.  See AnswerContext.
typedef B_FUNC
B_AnswerCallback(
        B_TRANSFER B_OPT struct B_Answer *,
        void *opaque,
        struct B_ErrorHandler const *);

B_ABSTRACT struct B_Question {
};

// Invariant: x.replicate() == x
// Invariant: x == y iff x.serialize() == y.serialize()
struct B_QuestionVTable {
    struct B_UUID uuid;
    struct B_AnswerVTable const *answer_vtable;

    B_FUNC
    (*answer)(
        struct B_Question const *,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);

    B_FUNC
    (*equal)(
        struct B_Question const *,
        struct B_Question const *,
        B_OUTPTR bool *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*replicate)(
        struct B_Question const *,
        B_OUTPTR struct B_Question **,
        struct B_ErrorHandler const *);

    B_FUNC
    (*deallocate)(
        B_TRANSFER struct B_Question *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*serialize)(
        struct B_Question const *,
        struct B_ByteSink *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*deserialize)(
        struct B_ByteSource *,
        B_OUTPTR struct B_Question **,
        struct B_ErrorHandler const *);
};

B_ABSTRACT struct B_Answer {
};

// Invariant: x.replicate() == x
// Invariant: x == y iff x.serialize() == y.serialize()
struct B_AnswerVTable {
    B_FUNC
    (*equal)(
        struct B_Answer const *,
        struct B_Answer const *,
        B_OUTPTR bool *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*replicate)(
        struct B_Answer const *,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);

    B_FUNC
    (*deallocate)(
        B_TRANSFER struct B_Answer *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*serialize)(
        struct B_Answer const *,
        struct B_ByteSink *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*deserialize)(
        struct B_ByteSource *,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);
};

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_question_serialize_to_memory(
        struct B_Question const *,
        struct B_QuestionVTable const *,
        B_OUTPTR uint8_t **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_answer_serialize_to_memory(
        struct B_Answer const *,
        struct B_AnswerVTable const *,
        B_OUTPTR uint8_t **data,
        B_OUT size_t *data_size,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_deserialize_from_memory(
        struct B_QuestionVTable const *,
        uint8_t const *data,
        size_t data_size,
        B_OUTPTR struct B_Question **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_answer_deserialize_from_memory(
        struct B_AnswerVTable const *,
        uint8_t const *data,
        size_t data_size,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Error.h>

template<typename TClass, typename TObject = TClass>
class B_QuestionClass;

template<typename TClass>
class B_AnswerClass :
        public B_Answer {
public:
    static B_AnswerVTable const vtable;

private:
    static B_FUNC
    equal_(
            B_Answer const *a,
            B_Answer const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, a);
        B_CHECK_PRECONDITION(eh, b);
        return TClass::equal(
            static_cast<TClass const *>(a),
            static_cast<TClass const *>(b),
            out,
            eh);
    }

    static B_FUNC
    replicate_(
            B_Answer const *answer,
            B_OUTPTR B_Answer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, answer);
        B_CHECK_PRECONDITION(eh, out);
        TClass *tmp;
        if (!TClass::replicate(
                static_cast<TClass const *>(answer),
                &tmp,
                eh)) {
            return false;
        }
        *out = tmp;
        return true;
    }

    static B_FUNC
    deallocate_(
            B_TRANSFER B_Answer *answer,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, answer);
        return TClass::deallocate(
            static_cast<TClass *>(answer), eh);
    }

    static B_FUNC
    serialize_(
            B_Answer const *answer,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, sink);
        B_CHECK_PRECONDITION(eh, answer);
        return TClass::serialize(
            static_cast<TClass const *>(answer), sink, eh);
    }

    static B_FUNC
    deserialize_(
            B_ByteSource *source,
            B_OUTPTR B_Answer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, source);
        B_CHECK_PRECONDITION(eh, out);
        TClass *tmp;
        if (!TClass::deserialize(source, &tmp, eh)) {
            return false;
        }
        *out = tmp;
        return true;
    }
};

template<typename TClass, typename TObject>
class B_QuestionClass :
        public B_Question {
public:
    typedef TClass Class;
    typedef TObject Object;

    static B_QuestionVTable const vtable;

private:
    static B_UUID uuid;

    static B_Question *
    cast_(TObject *p) {
        return static_cast<B_Question *>(
            static_cast<void *>(p));
    }

    static B_Question const *
    cast_(TObject const *p) {
        return static_cast<B_Question const *>(
            static_cast<void const *>(p));
    }

    static TObject *
    cast_(B_Question *p) {
        return static_cast<TObject *>(
            static_cast<void *>(p));
    }

    static TObject const *
    cast_(B_Question const *p) {
        return static_cast<TObject const *>(
            static_cast<void const *>(p));
    }

    static B_FUNC
    answer_(
            B_Question const *question,
            B_OUTPTR B_Answer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, question);
        B_CHECK_PRECONDITION(eh, out);
        typename TClass::AnswerClass *tmp;
        if (!TClass::answer(cast_(question), &tmp, eh)) {
            return false;
        }
        *out = tmp;
        return true;
    }

    static B_FUNC
    equal_(
            B_Question const *a,
            B_Question const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, a);
        B_CHECK_PRECONDITION(eh, b);
        return TClass::equal(cast_(a), cast_(b), out, eh);
    }

    static B_FUNC
    replicate_(
            B_Question const *question,
            B_OUTPTR B_Question **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, question);
        B_CHECK_PRECONDITION(eh, out);
        TObject *tmp;
        if (!TClass::replicate(cast_(question), &tmp, eh)) {
            return false;
        }
        *out = cast_(tmp);
        return true;
    }

    static B_FUNC
    deallocate_(
            B_TRANSFER B_Question *question,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, question);
        return TClass::deallocate(cast_(question), eh);
    }

    static B_FUNC
    serialize_(
            B_Question const *question,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, question);
        B_CHECK_PRECONDITION(eh, sink);
        return TClass::serialize(cast_(question), sink, eh);
    }

    static B_FUNC
    deserialize_(
            B_ByteSource *source,
            B_OUTPTR B_Question **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, source);
        B_CHECK_PRECONDITION(eh, out);
        TObject *tmp;
        if (!TClass::deserialize(source, &tmp, eh)) {
            return false;
        }
        *out = cast_(tmp);
        return true;
    }
};

#define B_QUESTION_ANSWER_CLASS_DEFINE_VTABLE( \
        question_class_name_, \
        question_class_uuid_) \
    B_ANSWER_CLASS_DEFINE_VTABLE_( \
        B_AnswerClass<question_class_name_::AnswerClass>); \
    B_QUESTION_CLASS_DEFINE_VTABLE_( \
        (question_class_uuid_), \
        B_QuestionClass< \
                question_class_name_, \
                question_class_name_::Object>);

#define B_QUESTION_CLASS_DEFINE_VTABLE_(uuid_, ...) \
    template<> \
    B_QuestionVTable const \
    __VA_ARGS__::vtable = { \
        (uuid_), \
        &__VA_ARGS__::Class::AnswerClass::vtable, \
        __VA_ARGS__::answer_, \
        __VA_ARGS__::equal_, \
        __VA_ARGS__::replicate_, \
        __VA_ARGS__::deallocate_, \
        __VA_ARGS__::serialize_, \
        __VA_ARGS__::deserialize_, \
    }

#define B_ANSWER_CLASS_DEFINE_VTABLE_(...) \
    template<> \
    B_AnswerVTable const \
    __VA_ARGS__::vtable = { \
        __VA_ARGS__::equal_, \
        __VA_ARGS__::replicate_, \
        __VA_ARGS__::deallocate_, \
        __VA_ARGS__::serialize_, \
        __VA_ARGS__::deserialize_, \
    }
#endif

#endif
