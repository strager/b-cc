#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_AnswerFuture;

struct B_PyAnswerFuture {
  PyObject_HEAD
  struct B_AnswerFuture *answer_future;
};

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_answer_future_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyAnswerFuture *
b_py_answer_future(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyAnswerFuture *
b_py_answer_future_from_pointer(
    B_TRANSFER struct B_AnswerFuture *);

#if defined(__cplusplus)
}
#endif
