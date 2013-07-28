#ifndef ANSWER_H_7CD1BFFA_4C9D_456D_BBD4_DB2DB42B480D
#define ANSWER_H_7CD1BFFA_4C9D_456D_BBD4_DB2DB42B480D

#include <stdbool.h>

struct AnswerVTable;

// An Answer is an object-oriented class describing the
// state of part of the system.  See 'struct AnyQuestion'.
struct AnyAnswer {
    struct AnswerVTable *vtable;
};

struct AnswerVTable {
    bool (*equal)(const struct AnyAnswer *, const struct AnyAnswer *);
    void (*deallocate)(struct AnyAnswer *);
};

// Compares two Answers for equality.  Returns 'true' if
// the two Questions are of the same type and are equal.
bool
b_answer_equal(const struct AnyAnswer *, const struct AnyAnswer *);

// Deallocates an Answer using its deallocation function.
// The Answer's associated vtable may be deallocated.
void
b_answer_deallocate(struct AnyAnswer *);

#endif
