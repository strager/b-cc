#ifndef BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3
#define BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3

#include <stddef.h>

struct B_BuildContext;
struct B_BuildDatabase;

// Creates a new BuildContext, building against the given
// BuildDatabase and using the given Rule for ensuring
// questions are answerable.  Does not take ownership of the
// given BuildDatabase or Rule.
struct B_BuildContext *
b_build_context_allocate(
    struct B_AnyBuildDatabase *,
    struct B_AnyRule *,
);

// Destroys a BuildContext.
void
b_build_context_deallocate(struct B_BuildContext *);

// Like b_build_context_need_answers, but without returning
// the answers.
void
b_build_context_need(
    const struct B_BuildContext *,
    struct B_AnyQuestion **,
    size_t,
);

// Requests some Questions be answered.  Uses the registered
// Rule for performing side effects.  May run Rules in
// parallel if multiple questions are asked.  Returns
// Answers corresponding to the Questions asked.  After
// calling, the Answers are owned by the caller.
void
b_build_context_need_answers(
    const struct B_BuildContext *,
    struct B_AnyQuestion **,
    struct B_AnyAnswer **,
    size_t,
);

#endif
