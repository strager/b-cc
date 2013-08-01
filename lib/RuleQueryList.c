#include "Allocate.h"
#include "RuleQueryList.h"

#include <stdlib.h>

struct B_RuleQueryListNode {
    struct B_RuleQuery query;
    struct B_RuleQueryListNode *next;  // Nullable.
};

struct B_RuleQueryList {
    struct B_RuleQueryListNode *head;  // Nullable.
    size_t size;
};

struct B_RuleQueryList *
b_rule_query_list_allocate() {
    B_ALLOCATE(struct B_RuleQueryList, list, {
        .head = NULL,
        .size = 0,
    });
    return list;
}

static void
b_rule_query_list_deallocate_node(
    struct B_RuleQueryListNode *node) {
    if (!node) {
        return;
    }
    b_rule_query_list_deallocate_node(node->next);
    node->query.deallocate_closure(node->query.closure);
    free(node);
}

void
b_rule_query_list_deallocate(
    struct B_RuleQueryList *list) {
    b_rule_query_list_deallocate_node(list->head);
    free(list);
}

void
b_rule_query_list_add(
    struct B_RuleQueryList *list,
    const struct B_RuleQuery *query) {
    B_ALLOCATE(struct B_RuleQueryListNode, node, {
        .query = *query,
        .next = list->head,
    });
    node->query.closure = node->query
        .replicate_closure(node->query.closure);
    list->head = node;
    ++list->size;
}

size_t
b_rule_query_list_size(
    struct B_RuleQueryList *list) {
    return list->size;
}

static const struct B_RuleQuery *
b_rule_query_list_node_get(
    struct B_RuleQueryListNode *node,
    size_t index) {
    if (index == 0) {
        return &node->query;
    } else {
        return b_rule_query_list_node_get(
            node->next,
            index - 1);
    }
}

const struct B_RuleQuery *
b_rule_query_list_get(
    struct B_RuleQueryList *list,
    size_t index) {
    if (index >= list->size) {
        return NULL;
    }

    return b_rule_query_list_node_get(list->head, index);
}
