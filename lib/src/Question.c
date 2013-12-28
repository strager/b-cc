#include <B/Answer.h>
#include <B/Internal/Validate.h>
#include <B/Question.h>
#include <B/UUID.h>

void
b_question_validate(
    const struct B_AnyQuestion *question) {
    B_VALIDATE(question);
}

void
b_question_vtable_validate(
    const struct B_QuestionVTable *vtable) {
    B_VALIDATE(vtable);
    b_uuid_validate(vtable->uuid);
    b_answer_vtable_validate(vtable->answer_vtable);
    B_VALIDATE(vtable->answer);
    B_VALIDATE(vtable->equal);
    B_VALIDATE(vtable->deallocate);
}