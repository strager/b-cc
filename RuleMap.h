#ifndef RULEMAP_H_5F01F491_1F10_4654_83F5_2DF76982EAE6
#define RULEMAP_H_5F01F491_1F10_4654_83F5_2DF76982EAE6

// A RuleMap is an abstract type for storing any types of
// Rules.  If multiple rules of the same type are added to a
// RuleMap, they are combined using b_rule_add.
struct B_RuleMap;

struct B_RuleMap *
b_rulemap_allocate();

void
b_rulemap_deallocate(
    struct B_RuleMap *,
);

void
b_rulemap_add(
    struct B_RuleMap *,
    const struct B_RuleVTable *,
    const struct B_AnyRule *,
);

struct B_AnyRule *
b_rulemap_to_rule(
    struct B_RuleMap *,
);

struct B_RuleMap *
b_rulemap_from_rule(
    struct B_AnyRule *,
);

const struct B_RuleVTable *
b_rulemap_vtable();

#endif
