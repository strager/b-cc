#include <B/Internal/Validate.h>
#include <B/Question.h>
#include <B/Rule.h>
#include <B/UUID.h>

void
b_rule_vtable_validate(
    const struct B_RuleVTable *vtable) {
    B_VALIDATE(vtable);
    b_uuid_validate(vtable->uuid);
    B_VALIDATE(vtable->add);
    B_VALIDATE(vtable->query);
    B_VALIDATE(vtable->deallocate);
}
