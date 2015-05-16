#include <B/Private/AnswerFuture.h>
#include <B/QuestionAnswer.h>

#include <cstddef>
#include <errno.h>
#include <gtest/gtest.h>

static B_FUNC void
b_dummy_answer_deallocate_(
    B_TRANSFER struct B_IAnswer *answer) {
  EXPECT_EQ(
    answer, *reinterpret_cast<struct B_IAnswer **>(answer));
}

struct B_AnswerVTable
b_dummy_answer_vtable_ = {
  b_dummy_answer_deallocate_,
  // TODO(strager)
  NULL,
  NULL,
  NULL,
  NULL,
};

TEST(TestAnswerFuture, PendingState) {
  struct B_Error e;

  struct B_AnswerFuture *future;
  ASSERT_TRUE(b_answer_future_allocate_one(
    &b_dummy_answer_vtable_, &future, &e));

  enum B_AnswerFutureState state;
  ASSERT_TRUE(b_answer_future_state(future, &state, &e));
  EXPECT_EQ(B_FUTURE_PENDING, state);
  size_t answer_count;
  EXPECT_FALSE(b_answer_future_answer_count(
    future, &answer_count, &e));
  EXPECT_EQ(EOPNOTSUPP, e.posix_error);
  struct B_IAnswer const *answer;
  EXPECT_FALSE(b_answer_future_answer(
    future, 0, &answer, &e));
  EXPECT_EQ(EOPNOTSUPP, e.posix_error);

  b_answer_future_release(future);
}

TEST(TestAnswerFuture, ResolvedState) {
  struct B_Error e;

  struct B_AnswerFuture *future;
  ASSERT_TRUE(b_answer_future_allocate_one(
    &b_dummy_answer_vtable_, &future, &e));
  struct B_IAnswer *dummy_answer
    = reinterpret_cast<struct B_IAnswer *>(&dummy_answer);
  ASSERT_TRUE(b_answer_future_resolve(
    future, dummy_answer, &e));

  enum B_AnswerFutureState state;
  ASSERT_TRUE(b_answer_future_state(future, &state, &e));
  EXPECT_EQ(B_FUTURE_RESOLVED, state);
  size_t answer_count;
  ASSERT_TRUE(b_answer_future_answer_count(
    future, &answer_count, &e));
  EXPECT_EQ(1U, answer_count);
  ASSERT_GT(answer_count, 0U);
  struct B_IAnswer const *answer;
  ASSERT_TRUE(b_answer_future_answer(
    future, 0, &answer, &e));
  EXPECT_EQ(dummy_answer, answer)
    << "Answer must not have been replicated";

  b_answer_future_release(future);
}

struct B_Closure_ {
  B_Closure_(
      struct B_AnswerFuture *future,
      struct B_IAnswer *answer,
      size_t *callback_called) :
      payload(0x123456789ABCDEF0ULL),
      future(future),
      answer(answer),
      callback_called(callback_called) {
  }

  // payload should be first to help test alignment.
  uint64_t payload;
  struct B_AnswerFuture *future;
  struct B_IAnswer *answer;
  size_t *callback_called;
};

static B_FUNC bool
b_resolved_callback_(
    B_BORROW struct B_AnswerFuture *future,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_Closure_ const *closure
    = static_cast<struct B_Closure_ const *>(callback_data);
  EXPECT_TRUE(closure);
  EXPECT_EQ(0x123456789ABCDEF0ULL, closure->payload);
  EXPECT_EQ(future, closure->future);
  *closure->callback_called += 1;

  enum B_AnswerFutureState state;
  EXPECT_TRUE(b_answer_future_state(future, &state, e));
  EXPECT_EQ(B_FUTURE_RESOLVED, state);
  size_t answer_count;
  EXPECT_TRUE(b_answer_future_answer_count(
    future, &answer_count, e));
  EXPECT_EQ(1U, answer_count);
  struct B_IAnswer const *answer;
  EXPECT_TRUE(b_answer_future_answer(
    future, 0, &answer, e));
  EXPECT_EQ(closure->answer, answer)
    << "Answer must not have been replicated";

  return true;
}

TEST(TestAnswerFuture, ResolvedPreCallback) {
  struct B_Error e;

  struct B_AnswerFuture *future;
  ASSERT_TRUE(b_answer_future_allocate_one(
    &b_dummy_answer_vtable_, &future, &e));
  size_t callback_called = 0;
  struct B_IAnswer *dummy_answer
    = reinterpret_cast<struct B_IAnswer *>(&dummy_answer);
  struct B_Closure_ closure(
    future, dummy_answer, &callback_called);
  ASSERT_TRUE(b_answer_future_add_callback(
    future,
    b_resolved_callback_,
    &closure,
    sizeof(closure),
    &e));
  EXPECT_EQ(0U, callback_called);
  ASSERT_TRUE(b_answer_future_resolve(
    future, dummy_answer, &e));
  EXPECT_EQ(1U, callback_called);
  b_answer_future_release(future);
  EXPECT_EQ(1U, callback_called);
}

TEST(TestAnswerFuture, ResolvedPostCallback) {
  struct B_Error e;

  struct B_AnswerFuture *future;
  ASSERT_TRUE(b_answer_future_allocate_one(
    &b_dummy_answer_vtable_, &future, &e));
  struct B_IAnswer *dummy_answer
    = reinterpret_cast<struct B_IAnswer *>(&dummy_answer);
  ASSERT_TRUE(b_answer_future_resolve(
    future, dummy_answer, &e));
  size_t callback_called = 0;
  struct B_Closure_ closure(
    future, dummy_answer, &callback_called);
  ASSERT_TRUE(b_answer_future_add_callback(
    future,
    b_resolved_callback_,
    &closure,
    sizeof(closure),
    &e));
  EXPECT_EQ(1U, callback_called);
  b_answer_future_release(future);
  EXPECT_EQ(1U, callback_called);
}

TEST(TestAnswerFuture, UnresolvedJoin) {
  struct B_Error e;

  struct B_AnswerFuture *futures[2];
  ASSERT_TRUE(b_answer_future_allocate_one(
    &b_dummy_answer_vtable_, &futures[0], &e));
  ASSERT_TRUE(b_answer_future_allocate_one(
    &b_dummy_answer_vtable_, &futures[1], &e));

  struct B_AnswerFuture *joined_future;
  ASSERT_TRUE(b_answer_future_join(
    futures, 2, &joined_future, &e));
  b_answer_future_release(joined_future);

  e.posix_error = EINVAL;
  ASSERT_TRUE(b_answer_future_fail(futures[0], e, &e));
  b_answer_future_release(futures[0]);
  e.posix_error = EINVAL;
  ASSERT_TRUE(b_answer_future_fail(futures[1], e, &e));
  b_answer_future_release(futures[1]);
}
