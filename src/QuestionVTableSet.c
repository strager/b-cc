#include <B/Alloc.h>
#include <B/Error.h>
#include <B/Macro.h>
#include <B/Private/Queue.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionVTableSet.h>
#include <B/UUID.h>

#include <errno.h>

struct VTableEntry_ {
    struct B_QuestionVTable const *B_CONST_STRUCT_MEMBER vtable;
    SLIST_ENTRY(VTableEntry_) link;
};

struct B_QuestionVTableSet {
    // FIXME(strager)
    SLIST_HEAD(foo, VTableEntry_) vtables;
};

B_EXPORT_FUNC
b_question_vtable_set_allocate(
        B_OUTPTR struct B_QuestionVTableSet **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    struct B_QuestionVTableSet *set;
    if (!b_allocate(sizeof(*set), (void **) &set, eh)) {
        return false;
    }
    *set = (struct B_QuestionVTableSet) {
        .vtables = LIST_HEAD_INITIALIZER(&set->vtables),
    };

    *out = set;
    return true;
}

B_EXPORT_FUNC
b_question_vtable_set_deallocate(
        B_TRANSFER struct B_QuestionVTableSet *set,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, set);
    bool ok = true;
    struct VTableEntry_ *vtable_entry;
    struct VTableEntry_ *vtable_entry_tmp;
    SLIST_FOREACH_SAFE(
            vtable_entry,
            &set->vtables,
            link,
            vtable_entry_tmp) {
        ok = b_deallocate(vtable_entry, eh) && ok;
    }
    ok = b_deallocate(set, eh) && ok;
    return ok;
}

B_EXPORT_FUNC
b_question_vtable_set_add(
        struct B_QuestionVTableSet *set,
        struct B_QuestionVTable const *vtable,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, set);
    B_CHECK_PRECONDITION(eh, vtable);

    struct VTableEntry_ *vtable_entry;
    if (!b_allocate(
            sizeof(*vtable_entry),
            (void **) &vtable_entry,
            eh)) {
        return false;
    }
    *vtable_entry = (struct VTableEntry_) {
        .vtable = vtable,
        // .link
    };
    SLIST_INSERT_HEAD(&set->vtables, vtable_entry, link);

    return true;
}

B_EXPORT_FUNC
b_question_vtable_set_look_up(
        struct B_QuestionVTableSet const *set,
        struct B_UUID uuid,
        B_OUTPTR B_BORROWED struct B_QuestionVTable const **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, set);
    B_CHECK_PRECONDITION(eh, out);

    struct VTableEntry_ *vtable_entry;
    SLIST_FOREACH(vtable_entry, &set->vtables, link) {
        struct B_QuestionVTable const *vtable
            = vtable_entry->vtable;
        if (b_uuid_equal(vtable->uuid, uuid)) {
            *out = vtable;
            return true;
        }
    }

    (void) B_RAISE_ERRNO_ERROR(
        eh, ENOENT, "b_question_vtable_set_look_up");
    return false;
}
