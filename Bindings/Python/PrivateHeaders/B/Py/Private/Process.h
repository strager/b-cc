#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_ProcessExitStatus;

struct B_PyProcessExitStatus;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_process_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyProcessExitStatus *
b_py_process_exit_status(
    B_BORROW PyObject *);

// Returns a reference to an instance of a *subclass* of
// B_PyProcessExitStatus.
B_WUR B_FUNC B_OUT_TRANSFER struct B_PyProcessExitStatus *
b_py_process_exit_status_from_pointer(
    B_BORROW struct B_ProcessExitStatus const *);

#if defined(__cplusplus)
}
#endif
