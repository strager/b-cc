#include <B/Error.h>
#include <B/Py/Private/Process.h>
#include <B/Py/Private/RunLoop.h>
#include <B/Py/Private/Util.h>
#include <B/RunLoop.h>

struct B_PyRunLoopImpl_ {
  struct B_RunLoopVTable vtable;
  struct B_PyRunLoop *run_loop_py;
};

struct B_PyRunLoop {
  PyObject_HEAD
  struct B_RunLoop *real_run_loop;
  struct B_PyRunLoopImpl_ run_loop;
};

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

B_WUR B_FUNC B_OUT_BORROW struct B_RunLoop *
b_py_run_loop_native(
    B_BORROW struct B_PyRunLoop *rl_py) {
  return (struct B_RunLoop *) &rl_py->run_loop;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyRunLoop *
b_py_run_loop_py(
    B_BORROW struct B_RunLoop *run_loop) {
  return ((struct B_PyRunLoopImpl_ *) run_loop)
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
  struct B_PyRunLoopImpl_ *rl
    = (struct B_PyRunLoopImpl_ *) run_loop;
  if (!b_run_loop_add_function(
      rl->run_loop_py->real_run_loop,
      callback,
      cancel_callback,
      callback_data,
      callback_data_size,
      e)) {
    return false;
  }
  return true;
}

static B_OUT_TRANSFER PyObject *
b_py_run_loop_allocate_(
    B_TRANSFER struct B_RunLoop *real_run_loop) {
  PyObject *self = b_py_run_loop_type_.tp_alloc(
    &b_py_run_loop_type_, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyRunLoop *rl_py = (struct B_PyRunLoop *) self;
  rl_py->real_run_loop = real_run_loop;
  rl_py->run_loop = (struct B_PyRunLoopImpl_) {
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

static PyObject *
b_py_run_loop_kqueue_(
    PyObject *cls,
    PyObject *args,
    PyObject *kwargs) {
  (void) cls;
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_RunLoop *rl;
  struct B_Error e;
  if (!b_run_loop_allocate_kqueue(&rl, &e)) {
    b_py_raise(e);
    return NULL;
  }
  return b_py_run_loop_allocate_(rl);
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
  struct B_RunLoop *rl;
  struct B_Error e;
  if (!b_run_loop_allocate_preferred(&rl, &e)) {
    b_py_raise(e);
    return NULL;
  }
  return b_py_run_loop_allocate_(rl);
}

static PyObject *
b_py_run_loop_sigchld_(
    PyObject *cls,
    PyObject *args,
    PyObject *kwargs) {
  (void) cls;
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_RunLoop *rl;
  struct B_Error e;
  if (!b_run_loop_allocate_sigchld(&rl, &e)) {
    b_py_raise(e);
    return NULL;
  }
  return b_py_run_loop_allocate_(rl);
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
  if (!b_run_loop_run(rl_py->real_run_loop, &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_RETURN_NONE;
}

struct B_PyRunLoopCallbackClosure_ {
  PyObject *callback;
};

static B_FUNC bool
b_py_run_loop_function_callback_(
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_PyRunLoopCallbackClosure_ const *closure
    = callback_data;
  bool ok;
  if (!PyCallable_Check(closure->callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    *e = b_py_error();
    goto fail;
  }
  PyObject *args = Py_BuildValue("()");
  if (!args) {
    *e = b_py_error();
    goto fail;
  }
  PyObject *result
    = PyEval_CallObject(closure->callback, args);
  Py_DECREF(args);
  if (!result) {
    *e = b_py_error();
    goto fail;
  }
  Py_DECREF(result);

  ok = true;
done:
  Py_DECREF(closure->callback);
  return ok;

fail:
  ok = false;
  goto done;
}

static B_FUNC bool
b_py_run_loop_process_callback_(
    B_BORROW struct B_ProcessExitStatus const *exit_status,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_PyRunLoopCallbackClosure_ const *closure
    = callback_data;
  bool ok;
  if (!PyCallable_Check(closure->callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    *e = b_py_error();
    goto fail;
  }
  struct B_PyProcessExitStatus *status_py
    = b_py_process_exit_status_from_pointer(exit_status);
  if (!status_py) {
    *e = b_py_error();
    goto fail;
  }
  PyObject *args = Py_BuildValue("(O)", status_py);
  Py_DECREF(status_py);
  if (!args) {
    *e = b_py_error();
    goto fail;
  }
  PyObject *result
    = PyEval_CallObject(closure->callback, args);
  Py_DECREF(args);
  if (!result) {
    *e = b_py_error();
    goto fail;
  }
  Py_DECREF(result);

  ok = true;
done:
  Py_DECREF(closure->callback);
  return ok;

fail:
  ok = false;
  goto done;
}

static B_FUNC bool
b_py_run_loop_cancel_callback_(
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  (void) e;
  struct B_PyRunLoopCallbackClosure_ const *closure
    = callback_data;
  Py_DECREF(closure->callback);
  return true;
}

static PyObject *
b_py_run_loop_add_function_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {"callback", NULL};
  PyObject *callback;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "O", keywords, &callback)) {
    return NULL;
  }
  if (!PyCallable_Check(callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    return NULL;
  }
  struct B_PyRunLoop *rl_py = b_py_run_loop(self);
  if (!rl_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_run_loop_add_function(
      rl_py->real_run_loop,
      b_py_run_loop_function_callback_,
      b_py_run_loop_cancel_callback_,
      &(struct B_PyRunLoopCallbackClosure_) {
        .callback = callback,
      },
      sizeof(struct B_PyRunLoopCallbackClosure_),
      &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_INCREF(rl_py);
  Py_INCREF(callback);
  Py_RETURN_NONE;
}

static PyObject *
b_py_run_loop_add_process_id_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  if (sizeof(PY_LONG_LONG) < sizeof(pid_t)) {
    // TODO(strager): static_assert.
    PyErr_SetString(
      PyExc_TypeError,
      "C long long is smaller than C pid_t");
    return NULL;
  }
  static char *keywords[]
    = {"process_id", "callback", NULL};
  PY_LONG_LONG pid;
  PyObject *callback;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "L" "O", keywords, &pid, &callback)) {
    return NULL;
  }
  if (!PyCallable_Check(callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    return NULL;
  }
  struct B_PyRunLoop *rl_py = b_py_run_loop(self);
  if (!rl_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_run_loop_add_process_id(
      rl_py->real_run_loop,
      // FIXME(strager): What about pid_t truncation?
      pid,
      b_py_run_loop_process_callback_,
      b_py_run_loop_cancel_callback_,
      &(struct B_PyRunLoopCallbackClosure_) {
        .callback = callback,
      },
      sizeof(struct B_PyRunLoopCallbackClosure_),
      &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_INCREF(rl_py);
  Py_INCREF(callback);
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
  if (!b_run_loop_stop(rl_py->real_run_loop, &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_RETURN_NONE;
}

static PyMethodDef
b_py_run_loop_methods_[] = {
  {
    .ml_name = "kqueue",
    .ml_meth = (PyCFunction) b_py_run_loop_kqueue_,
    .ml_flags = METH_CLASS | METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "preferred",
    .ml_meth = (PyCFunction) b_py_run_loop_preferred_,
    .ml_flags = METH_CLASS | METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "sigchld",
    .ml_meth = (PyCFunction) b_py_run_loop_sigchld_,
    .ml_flags = METH_CLASS | METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "add_function",
    .ml_meth = (PyCFunction) b_py_run_loop_add_function_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "add_process_id",
    .ml_meth = (PyCFunction) b_py_run_loop_add_process_id_,
    .ml_flags = METH_KEYWORDS,
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
  B_PY_METHOD_DEF_END
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
