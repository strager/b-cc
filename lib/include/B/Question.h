#ifndef QUESTION_H_D2583961_C888_4152_97C6_212BE8122C3F
#define QUESTION_H_D2583961_C888_4152_97C6_212BE8122C3F

#include <B/Serialize.h>
#include <B/UUID.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnswerVTable;
struct B_AnyAnswer;
struct B_Exception;

// A Question is value describing a question on the current
// state of the system.  For example, "is this file
// present?", "what is the content of this file?", and "what
// is this environmental variable set to?" are valid
// questions.  A Question is:
//
// 1. Only answerable after building using a Rule, or
// 2. A way to express a dependency on another question, or
// 3. Both (1) and (2).
//
// For example, the answer to the question "what is the
// content of this C object file?" is only answerable after
// a Rule which invokes a C compiler has executed.  In
// addition, that Rule may ask a question (using
// b_build_context_need) such as "what is the content of
// this C source file?", expressing a dependency on the C
// source file for building the C object file.
struct B_AnyQuestion {
};

// Virtual table for Questions.  See PATTERNS.md.
//
// Invariant: deserialize(a) == deserialize(b) iff a == b
// Invariant: deserialize(serialize(a)) == a
// Invariant: replicate(a) == a
struct B_QuestionVTable {
    struct B_UUID uuid;
    const struct B_AnswerVTable *answer_vtable;

    struct B_AnyAnswer *(*answer)(
        const struct B_AnyQuestion *,
        struct B_Exception **);

    bool (*equal)(
        const struct B_AnyQuestion *,
        const struct B_AnyQuestion *);

    struct B_AnyQuestion *(*replicate)(
        const struct B_AnyQuestion *);

    void (*deallocate)(
        struct B_AnyQuestion *);

    B_SerializeFunc serialize;
    B_DeserializeFunc0 deserialize;
};

void
b_question_validate(
    const struct B_AnyQuestion *);

void
b_question_vtable_validate(
    const struct B_QuestionVTable *);

#ifdef __cplusplus
}
#endif

#endif
