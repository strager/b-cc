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

// Like b_build_context_need_answers, but without returning
// the answers.
void
b_build_context_need(
    const struct BuildContext *,
    struct AnyQuestion **,
    size_t,
);

// Requests some Questions be answered.  Uses the registered
// Rule for performing side effects.  May run Rules in
// parallel if multiple questions are asked.  Returns
// Answers corresponding to the Questions asked.  After
// calling, the Answers are owned by the caller.
void
b_build_context_need_answers(
    const struct BuildContext *,
    struct AnyQuestion **,
    struct AnyAnswer **,
    size_t,
);

#endif
