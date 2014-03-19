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

template<typename T>
class B_QuestionClass;

template<typename T>
class B_AnswerClass :
        public B_Answer {
public:
    static B_FUNC
    equal(
            T const &,
            T const &,
            B_OUTPTR bool *,
            B_ErrorHandler const *);

    B_FUNC
    replicate(
            B_OUTPTR T **out,
            B_ErrorHandler const *eh) const {
        return static_cast<T const *>(this)
            ->replicate(out, eh);
    }

    B_FUNC
    deallocate(
            B_ErrorHandler const *eh) {
        return static_cast<T *>(this)->deallocate(eh);
    }

    B_FUNC
    serialize(
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) const {
        return static_cast<T const *>(this)
            ->serialize(out, eh);
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized,
            B_OUTPTR T **,
            B_ErrorHandler const *);

    static B_AnswerVTable const *
    vtable() {
        static B_AnswerVTable vtable = {
            // equal
            [](
                    B_Answer const *a,
                    B_Answer const *b,
                    B_OUTPTR bool *out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, a);
                B_CHECK_PRECONDITION(eh, b);
                return T::equal(
                    *static_cast<T const *>(a),
                    *static_cast<T const *>(b),
                    out,
                    eh);
            },
            // replicate
            [](
                    B_Answer const *answer,
                    B_OUTPTR B_Answer **out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, answer);
                B_CHECK_PRECONDITION(eh, out);
                T *tmp;
                if (!static_cast<T const *>(answer)
                        ->replicate(&tmp, eh)) {
                    return false;
                }
                *out = tmp;
                return true;
            },
            // deallocate
            [](
                    B_TRANSFER B_Answer *answer,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, answer);
                return static_cast<T *>(answer)
                    ->deallocate(eh);
            },
            // serialize
            [](
                    B_Answer const *answer,
                    B_OUT B_TRANSFER B_Serialized *out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, answer);
                return static_cast<T const *>(answer)
                    ->serialize(out, eh);
            },
            // deserialize
            [](
                    B_BORROWED B_Serialized serialized,
                    B_OUTPTR B_Answer **out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, out);
                T *tmp;
                if (!T::deserialize(serialized, &tmp, eh)) {
                    return false;
                }
                *out = tmp;
                return true;
            },
        };
        return &vtable;
    }
};

template<typename T>
class B_QuestionClass :
        public B_Question {
public:
    // HACK(strager): We would like to write the following
    // signature:
    //
    // B_FUNC
    // answer(
    //         B_OUTPTR typename T::AnswerClass **out,
    //         B_ErrorHandler const *eh) const {
    //
    // but C++ will not let us use a dependent type in a
    // function signature.  We can use it in the function
    // definition, however.
    template<typename TAnswerClass>
    B_FUNC
    answer(
            B_OUTPTR TAnswerClass **out,
            B_ErrorHandler const *eh) const {
        typename std::enable_if<std::is_same<TAnswerClass, typename T::AnswerClass>::value>::type *_;
        (void) _;
        return static_cast<T const *>(this)
            ->answer(out, eh);
    }

    static B_FUNC
    equal(
            T const &,
            T const &,
            B_OUTPTR bool *,
            B_ErrorHandler const *);

    B_FUNC
    replicate(
            B_OUTPTR T **out,
            B_ErrorHandler const *eh) const {
        return static_cast<T const *>(this)
            ->replicate(out, eh);
    }

    B_FUNC
    deallocate(
            B_ErrorHandler const *eh) {
        return static_cast<T *>(this)->deallocate(eh);
    }

    B_FUNC
    serialize(
            B_OUT B_Serialized *out,
            B_ErrorHandler const *eh) const {
        return static_cast<T *>(this)->serialize(out, eh);
    }

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized,
            B_OUTPTR T **,
            B_ErrorHandler const *);

    static B_QuestionVTable const *
    vtable() {
        static B_QuestionVTable vtable = {
            // uuid
            B_QuestionClass::uuid,
            // answer_vtable
            T::AnswerClass::vtable(),
            // answer
            [](
                    B_Question const *question,
                    B_OUTPTR B_Answer **out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, question);
                B_CHECK_PRECONDITION(eh, out);
                typename T::AnswerClass *tmp;
                if (!static_cast<T const *>(question)
                        ->answer(&tmp, eh)) {
                    return false;
                }
                *out = tmp;
                return true;
            },
            // equal
            [](
                    B_Question const *a,
                    B_Question const *b,
                    B_OUTPTR bool *out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, a);
                B_CHECK_PRECONDITION(eh, b);
                return T::equal(
                    *static_cast<T const *>(a),
                    *static_cast<T const *>(b),
                    out,
                    eh);
            },
            // replicate
            [](
                    B_Question const *question,
                    B_OUTPTR B_Question **out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, question);
                B_CHECK_PRECONDITION(eh, out);
                T *tmp;
                if (!static_cast<T const *>(question)
                        ->replicate(&tmp, eh)) {
                    return false;
                }
                *out = tmp;
                return true;
            },
            // deallocate
            [](
                    B_Question *question,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, question);
                return static_cast<T *>(question)
                    ->deallocate(eh);
            },
            // serialize
            [](
                    B_Question const *question,
                    B_OUT B_TRANSFER B_Serialized *out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, question);
                return static_cast<T const *>(question)
                    ->serialize(out, eh);
            },
            // deserialize
            [](
                    B_BORROWED B_Serialized serialized,
                    B_OUTPTR B_Question **out,
                    B_ErrorHandler const *eh) {
                B_CHECK_PRECONDITION(eh, out);
                T *tmp;
                if (!T::deserialize(serialized, &tmp, eh)) {
                    return false;
                }
                *out = tmp;
                return true;
            },
        };
        return &vtable;
    }

private:
    static B_UUID uuid;
};
#endif

#endif
