#pragma once

#include <B/Attributes.h>
#include <B/Private/AnswerFuture.h>

#include <stdbool.h>

struct B_AnswerFuture;
struct B_Database;
struct B_IQuestion;
struct B_QuestionVTable;

struct B_AnswerContext {
  struct B_Database *database;
  struct B_Main *main;
  struct B_IQuestion *question;
  struct B_QuestionVTable const *question_vtable;
  struct B_AnswerFuture *answer_future;
};

B_WUR B_EXPORT_FUNC bool
b_answer_context_allocate(
    B_BORROW struct B_Database *,
    B_BORROW struct B_Main *,
    B_BORROW struct B_IQuestion *,
    B_BORROW struct B_QuestionVTable const *,
    B_OUT_TRANSFER struct B_AnswerContext **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_context_deallocate(
    B_TRANSFER struct B_AnswerContext *,
    B_OUT struct B_Error *);

// For methods, see <B/AnswerContext.h>.
