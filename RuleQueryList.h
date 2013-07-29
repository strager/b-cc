#ifndef RULEQUERYLIST_H_E5F184CF_942B_466E_BD64_88AEFB3B5D85
#define RULEQUERYLIST_H_E5F184CF_942B_466E_BD64_88AEFB3B5D85

#include <stddef.h>

typedef void *B_RuleQueryClosure;

typedef void (*B_RuleQueryFunction)(
    struct B_BuildContext *,
    const struct B_AnyQuestion *,
    B_RuleQueryClosure,
    struct B_Exception **,
);

struct B_RuleQuery {
    B_RuleQueryFunction function;
    B_RuleQueryClosure closure;
    void (*deallocate_closure)(B_RuleQueryClosure *);
};

struct B_RuleQueryList;

struct B_RuleQueryList *
b_rule_query_list_allocate();

void
b_rule_query_list_deallocate(struct B_RuleQueryList *);

void
b_rule_query_list_append(
    struct B_RuleQueryList *,
    const struct B_RuleQuery,
);

size_t
b_rule_query_list_size(
    struct B_RuleQueryList *,
);

struct B_RuleQuery *
b_rule_query_list_get(
    struct B_RuleQueryList *,
    size_t index,
);

#endif
