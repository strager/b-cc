#include <B/Py/Private/RunLoop.h>

static bool
b_py_run_loop_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_run_loop_type) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop(
    B_BORROW PyObject *object) {
  if (!b_py_run_loop_check_(object)) {
    PyErr_SetString(PyExc_TypeError, "Expected RunLoop");
    return NULL;
  }
  return (struct B_PyRunLoop *) object;
}

B_WUR B_FUNC B_OUT_BORROW struct B_RunLoop *
b_py_run_loop_get(
    B_BORROW struct B_PyRunLoop *rl_py) {
  return (struct B_RunLoop *) &rl_py->run_loop;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop_py(
    B_BORROW struct B_RunLoop *run_loop) {
  return ((struct B_PyRunLoopImpl *) run_loop)
    ->run_loop_py;
}

PyTypeObject
b_py_run_loop_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyRunLoop),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_BASETYPE | Py_TPFLAGS_DEFAULT,
  .tp_name = "_b.RunLoop",
};

B_WUR B_FUNC bool
b_py_run_loop_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_run_loop_type) < 0) {
    return false;
  }
  Py_INCREF(&b_py_run_loop_type);
  if (PyModule_AddObject(
      module,
      "RunLoop",
      (PyObject *) &b_py_run_loop_type)) {
    Py_DECREF(&b_py_run_loop_type);
    return false;
  }
  return true;
}
