#include <B/Error.h>
#include <B/Main.h>
#include <B/Memory.h>
#include <B/Private/AnswerContext.h>
#include <B/Private/Assertions.h>
#include <B/Private/Database.h>
#include <B/Private/Log.h>
#include <B/Private/Main.h>
#include <B/Private/Memory.h>
#include <B/QuestionAnswer.h>
#include <B/RunLoop.h>

struct B_Main {
  struct B_Database *database;
  struct B_RunLoop *run_loop;
  B_MainCallback *callback;
  void *callback_opaque;
};

struct B_AnswerContextCallbackClosure_ {
  struct B_Main *main;
  struct B_AnswerContext *answer_context;
};

static B_FUNC bool
b_answer_context_callback_(
    B_BORROW struct B_AnswerFuture *future,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_PRECONDITION(callback_data);
  B_OUT_PARAMETER(e);

  struct B_AnswerContextCallbackClosure_ const *closure
    = callback_data;
  struct B_AnswerContext *ac = closure->answer_context;
  B_ASSERT(future == ac->answer_future);
  enum B_AnswerFutureState state;
  if (!b_answer_future_state(future, &state, e)) {
    return false;
  }
  switch (state) {
  case B_FUTURE_PENDING:
    B_NYI();  // B_BUG();?
    return false;
  case B_FUTURE_FAILED:
    // Nothing to do.
    return true;
  case B_FUTURE_RESOLVED:
    {
      size_t count;
      if (!b_answer_future_answer_count(
          future, &count, e)) {
        return false;
      }
      B_ASSERT(count == 1);
      struct B_IAnswer const *answer;
      if (!b_answer_future_answer(future, 0, &answer, e)) {
        return false;
      }
      if (!b_database_record_answer(
          closure->main->database,
          ac->question,
          ac->question_vtable,
          answer,
          e)) {
        return false;
      }
      return true;
    }
  }
  B_UNREACHABLE();
}

struct B_MainAnswerCallbackClosure_ {
  struct B_Main *main;
  struct B_AnswerContext *answer_context;
};

static B_FUNC bool
b_main_answer_callback_(
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(callback_data);
  B_OUT_PARAMETER(e);

  struct B_MainAnswerCallbackClosure_ const *closure
    = callback_data;
  if (!closure->main->callback(
      closure->main->callback_opaque,
      closure->main,
      closure->answer_context,
      e)) {
    return false;
  }
  return true;
}

static B_FUNC bool
b_main_answer_cancel_callback_(
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(callback_data);
  B_OUT_PARAMETER(e);

  struct B_MainAnswerCallbackClosure_ const *closure
    = callback_data;
  if (!b_answer_context_deallocate(
      closure->answer_context, e)) {
    return false;
  }
  return true;
}

static B_FUNC bool
b_main_cache_miss_callback_(
    B_BORROW struct B_Main *main,
    B_BORROW struct B_IQuestion *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_OUT_TRANSFER struct B_AnswerFuture **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(main);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  // First check the database.
  struct B_IAnswer *answer;
  if (!b_database_look_up_answer(
      main->database,
      question,
      question_vtable,
      &answer,
      e)) {
    return false;
  }
  if (answer) {
    struct B_AnswerFuture *future;
    if (!b_answer_future_allocate_one(
        question_vtable->answer_vtable, &future, e)) {
      B_NYI();  // TODO(strager): Clean up.
      return false;
    }
    if (!b_answer_future_resolve(future, answer, e)) {
      B_NYI();  // TODO(strager): Clean up.
      return false;
    }
    *out = future;
    return true;
  }

  struct B_AnswerContext *ac;
  if (!b_answer_context_allocate(
      main->database,
      main,
      question,
      question_vtable,
      &ac,
      e)) {
    return false;
  }
  struct B_AnswerContextCallbackClosure_ ac_closure = {
    .main = main,
    .answer_context = ac,
  };
  if (!b_answer_future_add_callback(
      ac->answer_future,
      b_answer_context_callback_,
      &ac_closure,
      sizeof(ac_closure),
      e)) {
    B_NYI();
    return false;
  }
  struct B_MainAnswerCallbackClosure_ callback_closure = {
    .main = main,
    .answer_context = ac,
  };
  if (!b_run_loop_add_function(
      main->run_loop,
      b_main_answer_callback_,
      b_main_answer_cancel_callback_,
      &callback_closure,
      sizeof(callback_closure),
      e)) {
    B_NYI();
    return false;
  }
  struct B_AnswerFuture *future = ac->answer_future;
  b_answer_future_retain(future);
  *out = future;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_main_allocate(
    B_BORROW struct B_Database *db,
    B_BORROW struct B_RunLoop *run_loop,
    B_BORROW B_MainCallback *callback,
    B_BORROW void *callback_opaque,
    B_OUT_TRANSFER struct B_Main **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(db);
  B_PRECONDITION(callback);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_Main *main;
  if (!b_allocate(sizeof(*main), (void **) &main, e)) {
    return false;
  }
  *main = (struct B_Main) {
    .database = db,
    .run_loop = run_loop,
    .callback = callback,
    .callback_opaque = callback_opaque,
  };
  *out = main;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_main_deallocate(
    B_TRANSFER struct B_Main *main,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(main);
  B_OUT_PARAMETER(e);

  b_deallocate(main);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_main_answer(
    B_BORROW struct B_Main *main,
    B_BORROW struct B_IQuestion *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_OUT_TRANSFER struct B_AnswerFuture **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(main);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_AnswerFuture *future;
  // TODO(strager): Implement caching so calls of
  // b_main_answer for the same question are serialized.
  if (!b_main_cache_miss_callback_(
      main, question, question_vtable, &future, e)) {
    return false;
  }
  *out = future;
  return true;
}

B_WUR B_FUNC B_BORROW struct B_RunLoop *
b_main_run_loop(
    B_BORROW struct B_Main *main) {
  B_PRECONDITION(main);

  return main->run_loop;
}
