#pragma once

#include <B/AnswerFuture.h>
#include <B/Attributes.h>
#include <B/Error.h>
#include <B/Private/Queue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct B_AnswerVTable;
struct B_Error;
struct B_IAnswer;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_answer_future_allocate_one(
    B_BORROW struct B_AnswerVTable const *,
    B_OUT_TRANSFER struct B_AnswerFuture **,
    B_OUT struct B_Error *);

// Invariant: future->answer_entry_count == 1.
B_WUR B_EXPORT_FUNC bool
b_answer_future_resolve(
    B_BORROW struct B_AnswerFuture *,
    B_TRANSFER struct B_IAnswer *,
    B_OUT struct B_Error *);

// Invariant: future->answer_entry_count == 1.
B_WUR B_EXPORT_FUNC bool
b_answer_future_fail(
    B_BORROW struct B_AnswerFuture *,
    struct B_Error,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_answer_future_join(
    B_BORROW struct B_AnswerFuture *const *,
    size_t future_count,
    B_OUT_TRANSFER struct B_AnswerFuture **,
    B_OUT struct B_Error *);

// For more methods, see <B/AnswerFuture.h>.

#if defined(__cplusplus)
}
#endif
