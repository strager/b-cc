#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_QuestionVTable;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_question_vtable_init(
    void);

B_WUR B_FUNC B_BORROW struct B_QuestionVTable const *
b_py_question_class_get_native_vtable(
    B_BORROW PyTypeObject *);

B_WUR B_FUNC B_BORROW struct B_QuestionVTable const *
b_py_question_class_get_python_vtable(
    B_BORROW PyTypeObject *);

B_WUR B_FUNC bool
b_py_question_class_set_native_vtable(
    B_BORROW PyTypeObject *,
    B_BORROW struct B_QuestionVTable const *);

#if defined(__cplusplus)
}
#endif
