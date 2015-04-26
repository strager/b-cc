#include <B/AnswerContext.h>
#include <B/Error.h>
#include <B/Main.h>
#include <B/Private/AnswerContext.h>
#include <B/Private/Assertions.h>
#include <B/Private/Database.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/QuestionAnswer.h>

B_WUR B_EXPORT_FUNC bool
b_answer_context_allocate(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_Main *main,
    B_TRANSFER struct B_IQuestion *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_OUT_TRANSFER struct B_AnswerContext **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(main);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_AnswerContext *ac;
  if (!b_allocate(sizeof(*ac), (void **) &ac, e)) {
    return false;
  }
  *ac = (struct B_AnswerContext) {
    .database = database,
    .main = main,
    .question = question,
    .question_vtable = question_vtable,
    // .answer_future
  };
  if (!b_answer_future_allocate_one(
      question_vtable->answer_vtable,
      &ac->answer_future,
      e)) {
    b_deallocate(ac);
    return false;
  }
  *out = ac;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_deallocate(
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(ac);
  B_OUT_PARAMETER(e);

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(
      ac->answer_future, &state, e)) {
    return false;
  }
  B_PRECONDITION(
    state == B_FUTURE_RESOLVED || state == B_FUTURE_FAILED);
  b_answer_future_release(ac->answer_future);
  b_deallocate(ac);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_question(
    B_BORROW struct B_AnswerContext *ac,
    B_OUT_BORROW struct B_IQuestion **out_question,
    B_OUT_BORROW struct B_QuestionVTable const **out_vtable,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(ac);
  B_OUT_PARAMETER(out_question);
  B_OUT_PARAMETER(out_vtable);
  B_OUT_PARAMETER(e);

  *out_question = ac->question;
  *out_vtable = ac->question_vtable;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_need_one(
    B_BORROW struct B_AnswerContext *ac,
    B_BORROW struct B_IQuestion *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_OUT_TRANSFER struct B_AnswerFuture **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(ac);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  if (!b_database_record_dependency(
      ac->database,
      ac->question,
      ac->question_vtable,
      question,
      question_vtable,
      e)) {
    return false;
  }
  struct B_AnswerFuture *answer_future;
  if (!b_main_answer(
      ac->main,
      question,
      question_vtable,
      &answer_future,
      e)) {
    return false;
  }
  *out = answer_future;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_need(
    B_BORROW struct B_AnswerContext *ac,
    B_BORROW struct B_IQuestion *const *questions,
    B_BORROW struct B_QuestionVTable const *const *vtables,
    size_t count,
    B_OUT_TRANSFER struct B_AnswerFuture **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(ac);
  B_PRECONDITION(questions);
  B_PRECONDITION(vtables);
  B_PRECONDITION(count > 0);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  bool ok;
  struct B_AnswerFuture **futures = NULL;

  if (!b_allocate2(
      sizeof(*futures),
      count,
      (void **) &futures,
      e)) {
    futures = NULL;
    goto fail;
  }
  for (size_t i = 0; i < count; ++i) {
    futures[i] = NULL;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!b_answer_context_need_one(
        ac, questions[i], vtables[i], &futures[i], e)) {
      futures[i] = NULL;
      goto fail;
    }
  }
  struct B_AnswerFuture *future;
  if (!b_answer_future_join(futures, count, &future, e)) {
    goto fail;
  }
  for (size_t i = 0; i < count; ++i) {
    b_answer_future_release(futures[i]);
  }
  *out = future;
  ok = true;

done:
  if (futures) {
    for (size_t i = 0; i < count; ++i) {
      if (futures[i]) {
        // TODO(strager)
      }
    }
    b_deallocate(futures);
  }
  return ok;

fail:
  ok = false;
  goto done;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_succeed(
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(ac);
  B_OUT_PARAMETER(e);

  struct B_IAnswer *answer = NULL;

  if (!ac->question_vtable->query_answer(
      ac->question, &answer, e)) {
    answer = NULL;
    goto fail;
  }
  if (!answer) {
    B_NYI();
  }
  if (!b_answer_context_succeed_answer(
      ac, answer, e)) {
    goto fail;
  }
  answer = NULL;
  return true;

fail:
  if (answer) {
    (void) ac->question_vtable->answer_vtable->deallocate(
      answer, &(struct B_Error) {});
  }
  return false;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_succeed_answer(
    B_TRANSFER struct B_AnswerContext *ac,
    B_TRANSFER struct B_IAnswer *answer,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(ac);
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(e);

  if (!b_answer_future_resolve(
      ac->answer_future, answer, e)) {
    return false;
  }
  if (!b_answer_context_deallocate(ac, e)) {
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_context_fail(
    B_TRANSFER struct B_AnswerContext *ac,
    struct B_Error error,
    B_OUT struct B_Error *e) {
  B_NYI();
  return false;
}
