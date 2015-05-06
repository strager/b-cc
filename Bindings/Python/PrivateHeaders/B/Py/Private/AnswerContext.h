#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_AnswerContext;
struct B_PyMain;

struct B_PyAnswerContext {
  PyObject_HEAD
  struct B_AnswerContext *answer_context;
  PyObject *main;
};

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_answer_context_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyAnswerContext *
b_py_answer_context(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyAnswerContext *
b_py_answer_context_from_pointer(
    B_TRANSFER struct B_AnswerContext *,
    B_BORROW struct B_PyMain *);

#if defined(__cplusplus)
}
#endif
