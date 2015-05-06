#include <B/RunLoop.h>
#include <B/Error.h>
#include <B/Py/Private/RunLoop.h>
#include <B/Py/Private/Util.h>

static PyTypeObject
b_py_run_loop_type_;

static bool
b_py_run_loop_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_run_loop_type_) == 1;
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

static PyObject *
b_py_run_loop_preferred_(
    PyObject *cls,
    PyObject *args,
    PyObject *kwargs) {
  (void) cls;

  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }

  PyObject *self = b_py_run_loop_type_.tp_alloc(
    &b_py_run_loop_type_, 0);
  if (!self) {
    return NULL;
  }

  struct B_PyRunLoop *rl_py = (struct B_PyRunLoop *) self;
  struct B_Error e;
  if (!b_run_loop_allocate_preferred(
      &rl_py->run_loop, &e)) {
    Py_DECREF(self);
    b_py_raise(e);
    return NULL;
  }
  return self;
}

static PyObject *
b_py_run_loop_run_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }

  struct B_PyRunLoop *rl_py = b_py_run_loop(self);
  if (!rl_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_run_loop_run(rl_py->run_loop, &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_RETURN_NONE;
}

static PyObject *
b_py_run_loop_stop_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }

  struct B_PyRunLoop *rl_py = b_py_run_loop(self);
  if (!rl_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_run_loop_stop(rl_py->run_loop, &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_RETURN_NONE;
}

static PyMethodDef
b_py_run_loop_methods_[] = {
  {
    .ml_name = "preferred",
    .ml_meth = (PyCFunction) b_py_run_loop_preferred_,
    .ml_flags = METH_CLASS | METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "run",
    .ml_meth = (PyCFunction) b_py_run_loop_run_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "stop",
    .ml_meth = (PyCFunction) b_py_run_loop_stop_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {},
};

static PyTypeObject
b_py_run_loop_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyRunLoop),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = b_py_run_loop_methods_,
  .tp_name = "_b.RunLoop",
};

B_WUR B_FUNC bool
b_py_run_loop_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_run_loop_type_) < 0) {
    return false;
  }
  Py_INCREF(&b_py_run_loop_type_);
  if (PyModule_AddObject(
      module,
      "RunLoop",
      (PyObject *) &b_py_run_loop_type_)) {
    Py_DECREF(&b_py_run_loop_type_);
    return false;
  }
  return true;
}
