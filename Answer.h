#ifndef ANSWER_H_7CD1BFFA_4C9D_456D_BBD4_DB2DB42B480D
#define ANSWER_H_7CD1BFFA_4C9D_456D_BBD4_DB2DB42B480D

#include <stdbool.h>

// An Answer is a value describing the state of part of the
// system.  See 'struct B_AnyQuestion'.
struct B_AnyAnswer {
};

struct B_AnswerVTable {
    bool (*equal)(
        const struct B_AnyAnswer *,
        const struct B_AnyAnswer *,
    );
    void (*deallocate)(
        struct B_AnyAnswer *,
    );
};

void
b_answer_validate(
    const struct B_AnyAnswer *,
);

void
b_answer_vtable_validate(
    const struct B_AnswerVTable *,
);

#endif
