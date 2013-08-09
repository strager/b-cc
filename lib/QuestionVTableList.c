#include "Allocate.h"
#include "Question.h"
#include "QuestionVTableList.h"

#include <stdlib.h>

struct B_QuestionVTableListNode {
    const struct B_QuestionVTable *question_vtable;
    struct B_QuestionVTableListNode *next;  // Nullable.
};

struct B_QuestionVTableList {
    struct B_QuestionVTableListNode *head;  // Nullable.
};

struct B_QuestionVTableList *
b_question_vtable_list_allocate(void) {
    B_ALLOCATE(struct B_QuestionVTableList, list, {
        .head = NULL,
    });
    return list;
}

static void
b_question_vtable_list_deallocate_node(
    struct B_QuestionVTableListNode *node) {
    if (!node) {
        return;
    }
    b_question_vtable_list_deallocate_node(node->next);
    free(node);
}

void
b_question_vtable_list_deallocate(
    struct B_QuestionVTableList *list) {
    b_question_vtable_list_deallocate_node(list->head);
    free(list);
}

void
b_question_vtable_list_add(
    struct B_QuestionVTableList *list,
    const struct B_QuestionVTable *question_vtable) {
    B_ALLOCATE(struct B_QuestionVTableListNode, node, {
        .question_vtable = question_vtable,
        .next = list->head,
    });
    list->head = node;
}

const struct B_QuestionVTable *
b_question_vtable_list_node_find_by_uuid(
    struct B_QuestionVTableListNode *node,
    struct B_UUID uuid) {
    if (!node) {
        return NULL;
    }

    const struct B_QuestionVTable *vtable
        = node->question_vtable;
    if (b_uuid_equal(uuid, vtable->uuid)) {
        return vtable;
    } else {
        return b_question_vtable_list_node_find_by_uuid(
            node->next,
            uuid);
    }
}

const struct B_QuestionVTable *
b_question_vtable_list_find_by_uuid(
    const struct B_QuestionVTableList *list,
    struct B_UUID uuid) {
    return b_question_vtable_list_node_find_by_uuid(
        list->head,
        uuid);
}
