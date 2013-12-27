#include <B/Database.h>
#include <B/Validate.h>

void
b_database_vtable_validate(
    const struct B_DatabaseVTable *vtable) {
    B_VALIDATE(vtable);
    B_VALIDATE(vtable->deallocate);
    B_VALIDATE(vtable->add_dependency);
    B_VALIDATE(vtable->get_answer);
    B_VALIDATE(vtable->set_answer);
}
