#pragma once

#include <B/Attributes.h>
#include <B/RunLoop.h>

#include <Python.h>
#include <stdbool.h>

struct B_PyRunLoopImpl {
  // Only has the fields set which the B library uses. For
  // PyRunLoopNative, forwards to the native RunLoop. For
  // other PyRunLoop subclasses, forwards to Python methods
  // calls.
  struct B_RunLoopVTable vtable;

  // Points to the containing PyRunLoop.
  // TODO(strager): Use an offsetof trick instead.
  struct B_PyRunLoop *run_loop_py;
};

struct B_PyRunLoop {
  PyObject_HEAD

  // RunLoop subclass given to the B library.
  struct B_PyRunLoopImpl run_loop;
};

#if defined(__cplusplus)
extern "C" {
#endif

extern PyTypeObject
b_py_run_loop_type;

B_WUR B_FUNC bool
b_py_run_loop_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC bool
b_py_run_loop_native_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_BORROW struct B_RunLoop *
b_py_run_loop_get(
    B_BORROW struct B_PyRunLoop *);

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop_py(
    B_BORROW struct B_RunLoop *);

#if defined(__cplusplus)
}
#endif
