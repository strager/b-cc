#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stddef.h>

struct B_AnswerFuture;
struct B_Error;
struct B_IAnswer;
struct B_IQuestion;
struct B_QuestionVTable;
struct B_RunLoop;

struct B_AnswerContext;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_answer_context_question(
    B_BORROW struct B_AnswerContext *,
    B_OUT_BORROW struct B_IQuestion **,
    B_OUT_BORROW struct B_QuestionVTable const **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_run_loop(
    B_BORROW struct B_AnswerContext *,
    B_OUT_BORROW struct B_RunLoop **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_need_one(
    B_BORROW struct B_AnswerContext *,
    B_BORROW struct B_IQuestion *,
    B_BORROW struct B_QuestionVTable const *,
    B_OUT_TRANSFER struct B_AnswerFuture **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_need(
    B_BORROW struct B_AnswerContext *,
    B_BORROW struct B_IQuestion *const *,
    B_BORROW struct B_QuestionVTable const *const *,
    size_t count,
    B_OUT_TRANSFER struct B_AnswerFuture **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_succeed(
    B_TRANSFER struct B_AnswerContext *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_succeed_answer(
    B_TRANSFER struct B_AnswerContext *,
    B_TRANSFER struct B_IAnswer *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_fail(
    B_TRANSFER struct B_AnswerContext *,
    struct B_Error,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
