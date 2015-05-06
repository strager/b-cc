#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_Database;

struct B_PyDatabase {
  PyObject_HEAD
  struct B_Database *database;
};

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_database_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyDatabase *
b_py_database(
    B_BORROW PyObject *);

#if defined(__cplusplus)
}
#endif
