#include "Question.h"
#include "Rule.h"
#include "UUID.h"
#include "Validate.h"

void
b_rule_vtable_validate(
    const struct B_RuleVTable *vtable) {
    B_VALIDATE(vtable);
    b_uuid_validate(vtable->uuid);
    b_question_vtable_validate(vtable->question_vtable);
    B_VALIDATE(vtable->add);
    B_VALIDATE(vtable->query);
    B_VALIDATE(vtable->deallocate);
}
