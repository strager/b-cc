#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_Main;

struct B_PyMain {
  PyObject_HEAD
  struct B_Main *main;
  PyObject *database;
  PyObject *run_loop;
  PyObject *callback;
};

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_main_init(
    B_BORROW PyObject *module);

#if defined(__cplusplus)
}
#endif
