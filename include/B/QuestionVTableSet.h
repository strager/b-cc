#ifndef B_HEADER_GUARD_2C7DDC2F_DABC_4632_A7EA_82EA526DC11F
#define B_HEADER_GUARD_2C7DDC2F_DABC_4632_A7EA_82EA526DC11F

#include <B/Base.h>
#include <B/UUID.h>

struct B_QuestionVTable;

// A collection of QuestionVTable-s.  Primarily used to look
// up a QuestionVTable by its UUID via
// b_question_vtable_set_look_up.
//
// Thread-safe: NO
// Signal-safe: NO
struct B_QuestionVTableSet;

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_question_vtable_set_allocate(
        B_OUTPTR struct B_QuestionVTableSet **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_vtable_set_deallocate(
        B_TRANSFER struct B_QuestionVTableSet *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_vtable_set_add(
        struct B_QuestionVTableSet *,
        struct B_QuestionVTable const *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_vtable_set_look_up(
        struct B_QuestionVTableSet const *,
        struct B_UUID,
        B_OUTPTR B_BORROWED struct B_QuestionVTable const **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/CXX.h>

struct B_QuestionVTableSetDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(B_QuestionVTableSet *vtables) {
        (void) b_question_vtable_set_deallocate(
            vtables,
            this->error_handler);
    }
};
#endif

#endif
