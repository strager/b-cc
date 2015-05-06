#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_Error;

struct B_PyIntConstant {
  char const *name;
  int value;
};

#if defined(__cplusplus)
extern "C" {
#endif

B_FUNC void
b_py_raise(
    struct B_Error);

B_FUNC struct B_Error
b_py_error(
    void);

B_FUNC struct B_Error
b_py_error_from(
    PyObject *);

B_WUR B_FUNC B_OUT_TRANSFER PyObject *
b_py_to_utf8_string(
    B_BORROW PyObject *);

B_WUR B_FUNC bool
b_py_add_int_constants(
    B_BORROW PyObject *dict,
    B_BORROW struct B_PyIntConstant const *);

#if defined(__cplusplus)
}
#endif
