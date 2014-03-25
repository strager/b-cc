#ifndef B_HEADER_GUARD_DB5E9C00_57E8_472F_9D69_9C7AB86C2F1E
#define B_HEADER_GUARD_DB5E9C00_57E8_472F_9D69_9C7AB86C2F1E

#include <B/Base.h>
#include <B/UUID.h>

#include <stdbool.h>

struct B_AnswerContext;
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

struct B_Serialized;

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
        B_OUT B_TRANSFER struct B_Serialized *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*deserialize)(
        B_BORROWED struct B_Serialized,
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
        B_OUT B_TRANSFER struct B_Serialized *,
        struct B_ErrorHandler const *);

    B_FUNC
    (*deserialize)(
        B_BORROWED struct B_Serialized,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);
};

struct B_Serialized {
    // Allocate data with b_allocate.  Deallocate data with
    // b_deallocate.
    void *data;
    size_t size;
};

#if defined(__cplusplus)
# include <B/Error.h>

template<typename TClass, typename TObject = TClass>
class B_QuestionClass;

template<typename TClass>
class B_AnswerClass :
        public B_Answer {
public:
    static B_FUNC
    equal(
            TClass const *,
            TClass const *,
            B_OUTPTR bool *,
            B_ErrorHandler const *);

    static B_FUNC
    replicate(
            TClass const *,
            B_OUTPTR TClass **,
            B_ErrorHandler const *);

    static B_FUNC
    deallocate(
            B_TRANSFER TClass *,
            B_ErrorHandler const *);

    static B_FUNC
    serialize(
            TClass const *,
            B_OUT B_Serialized *,
            B_ErrorHandler const *);

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized,
            B_OUTPTR TClass **,
            B_ErrorHandler const *);

    static B_AnswerVTable const *
    vtable() {
        static B_AnswerVTable vtable = {
            equal_,
            replicate_,
            deallocate_,
            serialize_,
            deserialize_,
        };
        return &vtable;
    }

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
            static_cast<TClass *>(answer),
            eh);
    }

    static B_FUNC
    serialize_(
            B_Answer const *answer,
            B_OUT B_TRANSFER B_Serialized *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, answer);
        return TClass::serialize(
            static_cast<TClass const *>(answer),
            out,
            eh);
    }

    static B_FUNC
    deserialize_(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR B_Answer **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);
        TClass *tmp;
        if (!TClass::deserialize(serialized, &tmp, eh)) {
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
    // HACK(strager): We would like to write the following
    // signature:
    //
    // static B_FUNC
    // answer(
    //         TObject const *,
    //         B_OUTPTR typename TClass::AnswerClass **,
    //         B_ErrorHandler const *) const;
    //
    // but C++ will not let us use a dependent type in a
    // function signature.
    template<typename TAnswerClass>
    static B_FUNC
    answer(
            TObject const *,
            B_OUTPTR TAnswerClass **,
            B_ErrorHandler const *);

    static B_FUNC
    equal(
            TObject const *,
            TObject const *,
            B_OUTPTR bool *,
            B_ErrorHandler const *);

    static B_FUNC
    replicate(
            TObject const *,
            B_OUTPTR TClass **,
            B_ErrorHandler const *);

    static B_FUNC
    deallocate(
            B_TRANSFER TObject *,
            B_ErrorHandler const *);

    static B_FUNC
    serialize(
            TObject const *,
            B_OUT B_Serialized *,
            B_ErrorHandler const *);

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized,
            B_OUTPTR TObject **,
            B_ErrorHandler const *);

    static B_QuestionVTable const *
    vtable() {
        static B_QuestionVTable vtable = {
            B_QuestionClass::uuid,
            TClass::AnswerClass::vtable(),
            answer_,
            equal_,
            replicate_,
            deallocate_,
            serialize_,
            deserialize_,
        };
        return &vtable;
    }

private:
    static B_UUID uuid;

    static B_Question *
    cast_(
            TObject *p) {
        return static_cast<B_Question *>(
            static_cast<void *>(p));
    }

    static B_Question const *
    cast_(
            TObject const *p) {
        return static_cast<B_Question const *>(
            static_cast<void const *>(p));
    }

    static TObject *
    cast_(
            B_Question *p) {
        return static_cast<TObject *>(
            static_cast<void *>(p));
    }

    static TObject const *
    cast_(
            B_Question const *p) {
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
            B_OUT B_TRANSFER B_Serialized *out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, question);
        return TClass::serialize(cast_(question), out, eh);
    }

    static B_FUNC
    deserialize_(
            B_BORROWED B_Serialized serialized,
            B_OUTPTR B_Question **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);
        TObject *tmp;
        if (!TClass::deserialize(serialized, &tmp, eh)) {
            return false;
        }
        *out = cast_(tmp);
        return true;
    }
};
#endif

#endif
