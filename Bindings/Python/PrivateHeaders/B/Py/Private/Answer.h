#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_IAnswer;

struct B_PyAnswer {
  PyObject_HEAD
  struct B_IAnswer *answer;
};

#if defined(__cplusplus)
extern "C" {
#endif

extern PyTypeObject
b_py_answer_type;

B_WUR B_FUNC bool
b_py_answer_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyAnswer *
b_py_answer(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_BORROW struct B_IAnswer *
b_py_ianswer(
    B_BORROW struct B_PyAnswer *);

// Returns a reference to an instance of a *subclass* of
// B_PyAnswer.
B_WUR B_FUNC B_OUT_TRANSFER struct B_PyAnswer *
b_py_answer_from_pointer(
    B_TRANSFER struct B_IAnswer *);

#if defined(__cplusplus)
}
#endif
