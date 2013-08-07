#ifndef RULEQUERYLIST_H_E5F184CF_942B_466E_BD64_88AEFB3B5D85
#define RULEQUERYLIST_H_E5F184CF_942B_466E_BD64_88AEFB3B5D85

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyQuestion;
struct B_BuildContext;
struct B_Exception;

typedef void (*B_RuleQueryFunction)(
    struct B_BuildContext *,
    const struct B_AnyQuestion *,
    void *closure,
    struct B_Exception **);

struct B_RuleQuery {
    B_RuleQueryFunction function;
    void *closure;
    void *(*replicate_closure)(
        const void *);
    void (*deallocate_closure)(
        void *);
};

struct B_RuleQueryList;

struct B_RuleQueryList *
b_rule_query_list_allocate();

void
b_rule_query_list_deallocate(
    struct B_RuleQueryList *);

void
b_rule_query_list_add(
    struct B_RuleQueryList *,
    const struct B_RuleQuery *);

size_t
b_rule_query_list_size(
    struct B_RuleQueryList *);

const struct B_RuleQuery *
b_rule_query_list_get(
    const struct B_RuleQueryList *,
    size_t index);

#ifdef __cplusplus
}
#endif

#endif
