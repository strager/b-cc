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
        struct B_Question *,
        struct B_ErrorHandler const *);
};

B_ABSTRACT struct B_Answer {
};

// Invariant: x.replicate() == x
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
        struct B_Answer const *,
        struct B_ErrorHandler const *);
};

#endif
