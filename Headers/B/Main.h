#pragma once

#include <B/Attributes.h>

#include <stdbool.h>

struct B_AnswerContext;
struct B_AnswerFuture;
struct B_Database;
struct B_Error;
struct B_IQuestion;
struct B_QuestionVTable;

struct B_Main;

typedef B_FUNC bool B_MainCallback(
    B_BORROW void *opaque,
    B_BORROW struct B_Main *,
    B_TRANSFER struct B_AnswerContext *,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_main_allocate(
    B_BORROW struct B_Database *,
    B_BORROW B_MainCallback *callback,
    B_BORROW void *callback_opaque,
    B_OUT_TRANSFER struct B_Main **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_main_deallocate(
    B_TRANSFER struct B_Main *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_main_answer(
    B_BORROW struct B_Main *main,
    B_BORROW struct B_IQuestion *,
    B_BORROW struct B_QuestionVTable const *,
    B_OUT_TRANSFER struct B_AnswerFuture **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_main_loop(
    B_BORROW struct B_Main *,
    B_OUT bool *keep_going,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
