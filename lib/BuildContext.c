#include "Allocate.h"
#include "Answer.h"
#include "BuildContext.h"
#include "Database.h"
#include "Exception.h"
#include "Question.h"
#include "Rule.h"
#include "RuleQueryList.h"
#include "Validate.h"

#include <stdlib.h>

struct B_BuildStack {
    const struct B_AnyQuestion *question;
    const struct B_QuestionVTable *question_vtable;
    const struct B_BuildStack *next;
};

struct B_BuildContextInfo {
    struct B_AnyDatabase *database;
    const struct B_DatabaseVTable *database_vtable;
    const struct B_AnyRule *rule;
    const struct B_RuleVTable *rule_vtable;
};

struct B_BuildContext {
    struct B_BuildContextInfo *const info;
    struct B_BuildStack *const stack;  // Nullable.
};

static struct B_BuildContext *
b_build_context_chain(
    const struct B_BuildContext *parent,
    const struct B_AnyQuestion *question,
    const struct B_QuestionVTable *question_vtable) {
    b_build_context_validate(parent);
    b_question_validate(question);
    b_question_vtable_validate(question_vtable);

    B_ALLOCATE(struct B_BuildStack, stack, {
        .question = question,
        .question_vtable = question_vtable,
        .next = parent->stack,
    });

    B_ALLOCATE(struct B_BuildContext, ctx, {
        .info = parent->info,
        .stack = stack,
    });

    return ctx;
}

void
b_build_context_unchain(
    struct B_BuildContext *ctx) {
    b_build_context_validate(ctx);

    free(ctx->stack);
    free(ctx);
}

struct B_BuildContext *
b_build_context_allocate(
    struct B_AnyDatabase *database,
    const struct B_DatabaseVTable *database_vtable,
    const struct B_AnyRule *rule,
    const struct B_RuleVTable *rule_vtable) {
    B_VALIDATE(database);
    b_database_vtable_validate(database_vtable);
    B_VALIDATE(rule);
    b_rule_vtable_validate(rule_vtable);

    B_ALLOCATE(struct B_BuildContextInfo, info, {
        .database = database,
        .database_vtable = database_vtable,
        .rule = rule,
        .rule_vtable = rule_vtable,
    });

    B_ALLOCATE(struct B_BuildContext, ctx, {
        .info = info,
        .stack = NULL,
    });

    return ctx;
}

void
b_build_context_deallocate(
    struct B_BuildContext *ctx) {
    b_build_context_validate(ctx);
    B_VALIDATE(!ctx->stack);

    free(ctx->info);
    free(ctx);
}

void
b_build_context_need(
    const struct B_BuildContext *ctx,
    const struct B_AnyQuestion *const *questions,
    const struct B_QuestionVTable *const *question_vtables,
    size_t count,
    struct B_Exception **ex) {
    b_build_context_validate(ctx);
    B_VALIDATE(questions);  /* TODO */
    B_VALIDATE(question_vtables);  /* TODO */

    // TODO Don't allocate/deallocate answers.
    struct B_AnyAnswer *answers[count];
    b_build_context_need_answers(
        ctx,
        questions,
        question_vtables,
        answers,
        count,
        ex);
    for (size_t i = 0; i < count; ++i) {
        question_vtables[i]->answer_vtable
            ->deallocate(answers[i]);
    }
}

void
b_build_context_need_answers(
    const struct B_BuildContext *ctx,
    const struct B_AnyQuestion *const *questions,
    const struct B_QuestionVTable *const *question_vtables,
    struct B_AnyAnswer **answers,
    size_t count,
    struct B_Exception **ex) {
    b_build_context_validate(ctx);
    B_VALIDATE(questions);  /* TODO */
    B_VALIDATE(question_vtables);  /* TODO */
    B_VALIDATE(answers);  /* TODO */

    // TODO Allow parallelism.
    struct B_Exception *exceptions[count];
    size_t exception_count = 0;
    for (size_t i = 0; i < count; ++i) {
        exceptions[exception_count] = NULL;
        answers[i] = b_build_context_need_answer_one(
            ctx,
            questions[i],
            question_vtables[i],
            exceptions + exception_count);
        if (exceptions[exception_count]) {
            ++exception_count;
        }
    }
    if (exception_count) {
        *ex = b_exception_aggregate(exceptions, exception_count);
    }
}

static void
b_build_context_build(
    struct B_BuildContext *ctx,
    const struct B_AnyQuestion *question,
    const struct B_QuestionVTable *question_vtable,
    struct B_Exception **ex) {
    b_build_context_validate(ctx);
    b_question_validate(question);

    const struct B_BuildContextInfo *info = ctx->info;

    struct B_RuleQueryList *rule_query_list
        = b_rule_query_list_allocate();

    info->rule_vtable->query(
        info->rule,
        question,
        question_vtable,
        ctx,
        rule_query_list,
        ex);
    B_EXCEPTION_THEN(ex, {
        goto done;
    });

    switch (b_rule_query_list_size(rule_query_list)) {
    case 0:
        *ex = b_exception_string("No rule to build question");
        goto done;

    case 1: {
        const struct B_RuleQuery *rule_query
            = b_rule_query_list_get(rule_query_list, 0);
        rule_query->function(
            ctx,
            question,
            rule_query->closure,
            ex);
        B_EXCEPTION_THEN(ex, {
            goto done;
        });
        break;
    }

    default:
        *ex = b_exception_string("Multiple rules to build question");
        goto done;
    }

done:
    b_rule_query_list_deallocate(rule_query_list);
}

struct B_AnyAnswer *
b_build_context_need_answer_one(
    const struct B_BuildContext *ctx,
    const struct B_AnyQuestion *question,
    const struct B_QuestionVTable *question_vtable,
    struct B_Exception **ex) {
    b_build_context_validate(ctx);
    b_question_validate(question);
    b_question_vtable_validate(question_vtable);

    const struct B_BuildContextInfo *info = ctx->info;

    const struct B_BuildStack *stack = ctx->stack;
    if (stack) {
        info->database_vtable->add_dependency(
            info->database,
            stack->question,
            stack->question_vtable,
            question,
            question_vtable,
            ex);
        B_EXCEPTION_THEN(ex, {
            return NULL;
        });
    }

    struct B_AnyAnswer *answer
        = info->database_vtable->get_answer(
            info->database,
            question,
            question_vtable,
            ex);
    if (answer) {
        return answer;
    }

    struct B_BuildContext *sub_ctx
        = b_build_context_chain(ctx, question, question_vtable);
    b_build_context_build(sub_ctx, question, question_vtable, ex);
    b_build_context_unchain(sub_ctx);
    B_EXCEPTION_THEN(ex, {
        return NULL;
    });

    answer = question_vtable->answer(question, ex);
    B_EXCEPTION_THEN(ex, {
        return NULL;
    });

    info->database_vtable->set_answer(
        info->database,
        question,
        question_vtable,
        answer,
        ex);
    B_EXCEPTION_THEN(ex, {
        return NULL;
    });

    return answer;
}

void
b_build_context_need_one(
    const struct B_BuildContext *ctx,
    const struct B_AnyQuestion *question,
    const struct B_QuestionVTable *question_vtable,
    struct B_Exception **ex) {
    b_build_context_validate(ctx);
    b_question_validate(question);
    b_question_vtable_validate(question_vtable);

    // TODO Don't allocate/deallocate answers.
    struct B_AnyAnswer *answer
        = b_build_context_need_answer_one(
            ctx,
            question,
            question_vtable,
            ex);
    if (answer) {
        question_vtable->answer_vtable
            ->deallocate(answer);
    }
    // Propagate exception.
}

static void
b_build_context_info_validate(
    const struct B_BuildContextInfo *info) {
    B_VALIDATE(info);
    B_VALIDATE(info->database);
    b_database_vtable_validate(info->database_vtable);
    B_VALIDATE(info->rule);
    b_rule_vtable_validate(info->rule_vtable);
}

static void
b_build_stack_validate(
    const struct B_BuildStack *stack) {
    if (!stack) {
        return;
    }
    b_question_validate(stack->question);
    b_question_vtable_validate(stack->question_vtable);
    b_build_stack_validate(stack->next);
}

void
b_build_context_validate(
    const struct B_BuildContext *ctx) {
    B_VALIDATE(ctx);
    b_build_context_info_validate(ctx->info);
    b_build_stack_validate(ctx->stack);
}
