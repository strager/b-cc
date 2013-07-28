#ifndef BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3
#define BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3

#include <stddef.h>

struct BuildContext;
struct BuildDatabase;

// Creates a new BuildContext, building against the given
// BuildDatabase and using the given Rule for ensuring
// questions are answerable.  Does not take ownership of the
// given BuildDatabase or Rule.
struct BuildContext *
b_build_context_allocate(
    struct AnyBuildDatabase *,
    struct AnyRule *,
);

// Destroys a BuildContext.
void
b_build_context_deallocate(struct BuildContext *);

// Requests some questions be answered.  Uses the registered
// Rule for performing side effects.  May run Rules in
// parallel if multiple questions are asked.
//
// The actual answer calculated is not returned; the side
// effect of answering (building) is the important part.
void
b_build_context_need(
    const struct BuildContext *,
    struct AnyQuestion **,
    size_t,
);

#endif
