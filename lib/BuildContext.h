#ifndef BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3
#define BUILDCONTEXT_H_860659D1_0C46_41B0_A9C1_EE04E32603D3

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyAnswer;
struct B_AnyDatabase;
struct B_AnyQuestion;
struct B_AnyRule;
struct B_DatabaseVTable;
struct B_Exception;
struct B_QuestionVTable;
struct B_RuleVTable;

struct B_BuildContext;

struct B_BuildContextInfo {
    struct B_AnyDatabase *database;
    const struct B_DatabaseVTable *database_vtable;
    const struct B_AnyRule *rule;
    const struct B_RuleVTable *rule_vtable;
};

// Creates a new BuildContext, building against the given
// Database and using the given Rule for ensuring questions
// are answerable.  Does not take ownership of the given
// Database or Rule.
struct B_BuildContext *
b_build_context_allocate(
    const struct B_BuildContextInfo *);

// Destroys a BuildContext.
void
b_build_context_deallocate(
    struct B_BuildContext *);

const struct B_BuildContextInfo *
b_build_context_info(
    const struct B_BuildContext *);

// Requests some Questions be answered.  Uses the registered
// Rule for performing side effects.  May run Rules in
// parallel if multiple questions are asked.  Returns
// Answers corresponding to the Questions asked.  After
// calling, the Answers are owned by the caller.
void
b_build_context_need_answers(
    const struct B_BuildContext *,
    const struct B_AnyQuestion *const *,
    const struct B_QuestionVTable *const *,
    struct B_AnyAnswer **,
    size_t count,
    struct B_Exception **);

// Like b_build_context_need_answers, but without returning
// the answers.
void
b_build_context_need(
    const struct B_BuildContext *,
    const struct B_AnyQuestion *const *,
    const struct B_QuestionVTable *const *,
    size_t count,
    struct B_Exception **);

struct B_AnyAnswer *
b_build_context_need_answer_one(
    const struct B_BuildContext *,
    const struct B_AnyQuestion *,
    const struct B_QuestionVTable *,
    struct B_Exception **);

void
b_build_context_need_one(
    const struct B_BuildContext *,
    const struct B_AnyQuestion *,
    const struct B_QuestionVTable *,
    struct B_Exception **);

void
b_build_context_validate(
    const struct B_BuildContext *);

#ifdef __cplusplus
}
#endif

#endif