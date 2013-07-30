#ifndef RULE_H_47BACA82_A85E_47FA_92B8_8489B9DAAD3A
#define RULE_H_47BACA82_A85E_47FA_92B8_8489B9DAAD3A

#include "RuleQueryList.h"
#include "UUID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct B_AppendOnlyList;
struct B_BuildContext;
struct B_QuestionVTable;

// A Rule is a structure with an interface for performing a
// build.  Its purpose is to ensure a Question is
// answerable.
struct B_AnyRule {
};

// Virtual table for Rules.  See PATTERNS.md.
struct B_RuleVTable {
    struct B_UUID uuid;
    struct B_QuestionVTable *questionVTable;
    void (*add)(struct B_AnyRule *, const struct B_AnyRule *);
    void (*query)(
        const struct B_AnyRule *,
        const struct B_AnyQuestion *,
        const struct B_BuildContext *,
        struct B_RuleQueryList *,
        struct B_Exception **);
    void (*deallocate)(struct B_AnyRule *);
};

void
b_rule_vtable_validate(
    const struct B_RuleVTable *);

#ifdef __cplusplus
}
#endif

#endif
