#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_IQuestion;
struct B_QuestionVTable;

struct B_PyQuestion {
  PyObject_HEAD
  struct B_IQuestion *question;
};

#if defined(__cplusplus)
extern "C" {
#endif

extern PyTypeObject
b_py_question_type;

B_WUR B_FUNC bool
b_py_question_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyQuestion *
b_py_question(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_BORROW struct B_IQuestion *
b_py_iquestion(
    B_BORROW struct B_PyQuestion *);

B_WUR B_FUNC B_BORROW struct B_QuestionVTable const *
b_py_question_python_vtable(
    B_BORROW struct B_PyQuestion *);

// Returns a reference to an instance of a *subclass* of
// B_PyQuestion.
B_WUR B_FUNC B_OUT_TRANSFER struct B_PyQuestion *
b_py_question_from_pointer(
    B_TRANSFER struct B_IQuestion *);

#if defined(__cplusplus)
}
#endif
