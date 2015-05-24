#include <B/Error.h>
#include <B/Py/Private/RunLoop.h>
#include <B/Py/Private/Util.h>

static PyTypeObject
b_py_run_loop_function_type_;

struct B_PyRunLoopFunction_ {
  PyObject_HEAD
  B_RunLoopFunction *callback;
  B_RunLoopFunction *cancel_callback;
  void *callback_data;
};

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

static B_WUR B_FUNC bool
b_py_run_loop_vtable_add_function_(
    B_BORROW struct B_RunLoop *run_loop,
    B_TRANSFER B_RunLoopFunction *callback,
    B_TRANSFER B_RunLoopFunction *cancel_callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  struct B_PyRunLoopImpl *rl
    = (struct B_PyRunLoopImpl *) run_loop;
  struct B_PyRunLoopFunction_ *function_py
    = (struct B_PyRunLoopFunction_ *)
      b_py_run_loop_function_type_.tp_alloc(
        &b_py_run_loop_function_type_, 0);
  if (!function_py) {
    *e = b_py_error();
    return false;
  }
  function_py->callback = callback;
  function_py->cancel_callback = cancel_callback;
  if (callback_data) {
    // TODO(strager): Use tp_alloc's nitems and tp_itemsize
    // instead of an extra heap allocation.
    // TODO(strager): Assert proper alignment.
    void *callback_data_copy
      = PyMem_Malloc(callback_data_size);
    if (!callback_data_copy) {
      Py_DECREF(function_py);
      *e = (struct B_Error) {.posix_error = ENOMEM};
      return false;
    }
    memcpy(
      callback_data_copy,
      callback_data,
      callback_data_size);
    function_py->callback_data = callback_data_copy;
  } else {
    function_py->callback_data = NULL;
  }
  struct B_PyRunLoop *rl_py = rl->run_loop_py;
  PyObject *result = PyObject_CallMethod(
    (PyObject *) rl_py, "add_function", "O", function_py);
  // FIXME(strager): Should the cancel callback run on
  // failure? If so, should it run immediately or be
  // scheduled?
  Py_DECREF(function_py);
  if (!result) {
    *e = b_py_error();
    return false;
  }
  return true;
}

static PyObject *
b_py_run_loop_new_(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs) {
  (void) args;
  (void) kwargs;
  PyObject *self = type->tp_alloc(type, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyRunLoop *rl_py = (struct B_PyRunLoop *) self;
  rl_py->run_loop = (struct B_PyRunLoopImpl) {
    .vtable = {
      .deallocate = NULL,
      .add_function = b_py_run_loop_vtable_add_function_,
      .add_process_id = NULL,
      .run = NULL,
      .stop = NULL,
    },
    .run_loop_py = rl_py,
  };
  return self;
}

PyTypeObject
b_py_run_loop_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyRunLoop),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_BASETYPE | Py_TPFLAGS_DEFAULT,
  .tp_name = "_b.RunLoop",
  .tp_new = b_py_run_loop_new_,
};

static bool
b_py_run_loop_function_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object,
    (PyObject *) &b_py_run_loop_function_type_) == 1;
}

static B_WUR B_FUNC B_OUT_BORROW
  struct B_PyRunLoopFunction_ *
b_py_run_loop_function_(
    B_BORROW PyObject *object) {
  if (!b_py_run_loop_function_check_(object)) {
    PyErr_SetString(
      PyExc_TypeError, "Expected RunLoopFunction_");
    return NULL;
  }
  return (struct B_PyRunLoopFunction_ *) object;
}

static PyObject *
b_py_run_loop_function_call_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyRunLoopFunction_ *function_py
    = b_py_run_loop_function_(self);
  if (!function_py) {
    return NULL;
  }
  struct B_Error e;
  if (!function_py->callback(
      function_py->callback_data, &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_RETURN_NONE;
}

static PyTypeObject
b_py_run_loop_function_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyRunLoopFunction_),
  .tp_call = b_py_run_loop_function_call_,
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_name = "_b.RunLoopFunction_",
};

B_WUR B_FUNC bool
b_py_run_loop_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_run_loop_function_type_) < 0) {
    return false;
  }
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
