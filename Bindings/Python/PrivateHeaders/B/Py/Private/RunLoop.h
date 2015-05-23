#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_PyRunLoop;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_run_loop_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_BORROW struct B_RunLoop *
b_py_run_loop_native(
    B_BORROW struct B_PyRunLoop *);

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop_py(
    B_BORROW struct B_RunLoop *);

#if defined(__cplusplus)
}
#endif
