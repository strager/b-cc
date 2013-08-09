#ifndef QUESTIONVTABLELIST_H_78C7FC53_2F3C_4A90_8E69_E17F4F250C59
#define QUESTIONVTABLELIST_H_78C7FC53_2F3C_4A90_8E69_E17F4F250C59

#include "UUID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct B_QuestionVTable;

struct B_QuestionVTableList;

struct B_QuestionVTableList *
b_question_vtable_list_allocate(void);

void
b_question_vtable_list_deallocate(
    struct B_QuestionVTableList *);

void
b_question_vtable_list_add(
    struct B_QuestionVTableList *,
    const struct B_QuestionVTable *);

const struct B_QuestionVTable *
b_question_vtable_list_find_by_uuid(
    const struct B_QuestionVTableList *,
    struct B_UUID);

#ifdef __cplusplus
}
#endif

#endif
