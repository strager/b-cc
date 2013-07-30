#include "Answer.h"
#include "Validate.h"

void
b_answer_validate(
    const struct B_AnyAnswer *answer) {
    B_VALIDATE(answer);
}

void
b_answer_vtable_validate(
    const struct B_AnswerVTable *vtable) {
    B_VALIDATE(vtable);
    B_VALIDATE(vtable->equal);
    B_VALIDATE(vtable->deallocate);
}
