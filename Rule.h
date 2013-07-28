#ifndef RULE_H_47BACA82_A85E_47FA_92B8_8489B9DAAD3A
#define RULE_H_47BACA82_A85E_47FA_92B8_8489B9DAAD3A

#include "UUID.h"

struct AppendOnlyList;
struct BuildContext;
struct RuleVTable;

// A Rule is an object-oriented class for performing a
// build.  Its purpose is to ensure a Question is
// answerable.
struct AnyRule {
    struct RuleVTable *vtable;
};

typedef void (*RuleQueryFunc)(
    struct BuildContext *,
    const struct AnyQuestion *,
    void *,  // User closure.
);

struct RuleVTable {
    UUID uuid;
    UUID questionUUID;
    void (*add)(struct AnyRule *, const struct AnyRule *);
    void (*query)(
        const struct AnyRule *,
        const struct AnyQuestion *,
        const struct BuildContext *,
        struct RuleQueryList *,
    );
    void (*deallocate)(struct AnyRule *);
};

// Combines two rules of the same type, leaving the result
// in the first rule via mutation.
void
b_rule_add(struct AnyRule *, const struct AnyRule *);

// Finds all RuleQueryFuncs which can be used to help answer
// the given Question.  In querying, questions may be asked
// using b_build_context_need.
void
b_rule_query(
    const struct AnyRule *,
    const struct AnyQuestion *,
    const struct BuildContext *,
    struct RuleQueryList *,
);

// Deallocates a Rule using its deallocation function.  The
// Rule's associated vtable may be deallocated.
void
b_rule_dealloate(struct AnyRule *)

#endif
