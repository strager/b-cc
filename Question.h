#ifndef QUESTION_H_D2583961_C888_4152_97C6_212BE8122C3F
#define QUESTION_H_D2583961_C888_4152_97C6_212BE8122C3F

#include "UUID.h"

#include <stdbool.h>

struct AnswerVTable;
struct AnyAnswer;
struct QuestionVTable;

// A Question is an object-oriented class describing a
// question on the current state of the system.  For
// example, "is this file present?", "what is the content of
// this file?", and "what is this environmental variable set
// to?" are valid questions.  A Question is either:
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
struct AnyQuestion {
    struct QuestionVTable *vtable;
};

struct QuestionVTable {
    struct UUID uuid;
    struct AnswerVTable *answerVTable;
    struct AnyAnswer *(*answer)(const struct AnyQuestion *);
    bool (*equal)(const struct AnyQuestion *, const struct AnyQuestion *);
    void (*deallocate)(struct AnyQuestion *);
};

// Answers a Question.  Returns NULL if the question cannot
// be answered.
struct AnyAnswer *
b_question_answer(const struct AnyQuestion *);

// Compares two Questions for equality.  Returns 'true' if
// the two Questions are of the same type and are equal.
bool
b_question_equal(const struct AnyQuestion *, const struct AnyQuestion *);

// Deallocates a Question using its deallocation function.
// The Question's associated vtable may be deallocated.
void
b_question_deallocate(struct AnyQuestion *);

#endif
