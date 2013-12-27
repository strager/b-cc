#ifndef ANSWER_H_7CD1BFFA_4C9D_456D_BBD4_DB2DB42B480D
#define ANSWER_H_7CD1BFFA_4C9D_456D_BBD4_DB2DB42B480D

#include <B/Serialize.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// An Answer is a value describing the state of part of the
// system.  See 'struct B_AnyQuestion'.
struct B_AnyAnswer {
};

// Virtual table for Answers.  See PATTERNS.md.
//
// Invariant: deserialize(a) == deserialize(b) iff a == b
// Invariant: deserialize(serialize(a)) == a
// Invariant: replicate(a) == a
struct B_AnswerVTable {
    bool (*equal)(
        const struct B_AnyAnswer *,
        const struct B_AnyAnswer *);

    struct B_AnyAnswer *(*replicate)(
        const struct B_AnyAnswer *);

    void (*deallocate)(
        struct B_AnyAnswer *);

    B_SerializeFunc serialize;
    B_DeserializeFunc0 deserialize;
};

void
b_answer_validate(
    const struct B_AnyAnswer *);

void
b_answer_vtable_validate(
    const struct B_AnswerVTable *);

#ifdef __cplusplus
}
#endif

#endif
