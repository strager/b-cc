#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_file_question_init(
    B_BORROW PyObject *module);

#if defined(__cplusplus)
}
#endif
