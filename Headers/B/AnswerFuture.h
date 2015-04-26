#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stddef.h>

struct B_Error;
struct B_IAnswer;

struct B_AnswerFuture;

enum B_AnswerFutureState {
  B_FUTURE_PENDING = 1,
  B_FUTURE_RESOLVED = 2,
  B_FUTURE_FAILED = 3,
};

typedef B_FUNC bool
B_AnswerFutureCallback(
    B_BORROW struct B_AnswerFuture *,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_answer_future_state(
    B_BORROW struct B_AnswerFuture *,
    B_OUT enum B_AnswerFutureState *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_future_answer_count(
    B_BORROW struct B_AnswerFuture *,
    B_OUT size_t *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_future_answer(
    B_BORROW struct B_AnswerFuture *,
    size_t index,
    B_OUT_BORROW struct B_IAnswer const **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_future_add_callback(
    B_BORROW struct B_AnswerFuture *,
    B_TRANSFER B_AnswerFutureCallback *callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC void
b_answer_future_retain(
    B_BORROW struct B_AnswerFuture *);

B_WUR B_EXPORT_FUNC void
b_answer_future_release(
    B_TRANSFER struct B_AnswerFuture *);

#if defined(__cplusplus)
}
#endif
