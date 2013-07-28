#ifndef RULE_H_47BACA82_A85E_47FA_92B8_8489B9DAAD3A
#define RULE_H_47BACA82_A85E_47FA_92B8_8489B9DAAD3A

#include "UUID.h"

struct B_AppendOnlyList;
struct B_BuildContext;
struct B_RuleVTable;

// A Rule is an object-oriented class for performing a
// build.  Its purpose is to ensure a Question is
// answerable.
struct B_AnyRule {
    struct B_RuleVTable *vtable;
};

typedef void (*RuleQueryFunc)(
    struct B_BuildContext *,
    const struct B_AnyQuestion *,
    void *,  // User closure.
);

struct B_RuleVTable {
    UUID uuid;
    UUID questionUUID;
    void (*add)(struct B_AnyRule *, const struct B_AnyRule *);
    void (*query)(
        const struct B_AnyRule *,
        const struct B_AnyQuestion *,
        const struct B_BuildContext *,
        struct B_RuleQueryList *,
    );
    void (*deallocate)(struct B_AnyRule *);
};

// Combines two rules of the same type, leaving the result
// in the first rule via mutation.
void
b_rule_add(struct B_AnyRule *, const struct B_AnyRule *);

// Finds all RuleQueryFuncs which can be used to help answer
// the given Question.  In querying, questions may be asked
// using b_build_context_need.
void
b_rule_query(
    const struct B_AnyRule *,
    const struct B_AnyQuestion *,
    const struct B_BuildContext *,
    struct B_RuleQueryList *,
);

// Deallocates a Rule using its deallocation function.  The
// Rule's associated vtable may be deallocated.
void
b_rule_dealloate(struct B_AnyRule *)

#endif
