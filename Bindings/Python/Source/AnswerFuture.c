#include <B/AnswerFuture.h>
#include <B/Error.h>
#include <B/Py/Private/AnswerFuture.h>
#include <B/Py/Private/Util.h>

static PyTypeObject
b_py_answer_future_type_;

static bool
b_py_answer_future_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_answer_future_type_) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyAnswerFuture *
b_py_answer_future(
    B_BORROW PyObject *object) {
  if (!b_py_answer_future_check_(object)) {
    PyErr_SetString(
      PyExc_TypeError, "Expected AnswerFuture");
    return NULL;
  }
  return (struct B_PyAnswerFuture *) object;
}

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyAnswerFuture *
b_py_answer_future_from_pointer(
    B_TRANSFER struct B_AnswerFuture *answer_future) {
  PyObject *self = b_py_answer_future_type_.tp_alloc(
    &b_py_answer_future_type_, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyAnswerFuture *future_py
    = (struct B_PyAnswerFuture *) self;
  future_py->answer_future = answer_future;
  return future_py;
}

struct B_PyAnswerFutureClosure_ {
  struct B_PyAnswerFuture *future_py;
  PyObject *callback;
};

static B_FUNC bool
b_py_answer_future_callback_(
    B_BORROW struct B_AnswerFuture *future,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_PyAnswerFutureClosure_ const *closure
    = callback_data;
  bool ok;
  assert(closure->future_py->answer_future == future);
  if (!PyCallable_Check(closure->callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    *e = b_py_error();
    goto fail;
  }
  PyObject *args
    = Py_BuildValue("(O)", (PyObject *) closure->future_py);
  if (!args) {
    *e = b_py_error();
    goto fail;
  }
  PyObject *result = PyEval_CallObject(
    closure->callback, args);
  Py_DECREF(args);
  if (!result) {
    *e = b_py_error();
    goto fail;
  }
  Py_DECREF(result);

  ok = true;
done:
  Py_DECREF(closure->future_py);
  Py_DECREF(closure->callback);
  return ok;

fail:
  ok = false;
  goto done;
}

static PyObject *
b_py_answer_future_add_callback_(
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
  struct B_PyAnswerFuture *future_py
    = b_py_answer_future(self);
  if (!future_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_answer_future_add_callback(
      future_py->answer_future,
      b_py_answer_future_callback_,
      &(struct B_PyAnswerFutureClosure_) {
        .future_py = future_py,
        .callback = callback,
      },
      sizeof(struct B_PyAnswerFutureClosure_),
      &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_INCREF(future_py);
  Py_INCREF(callback);
  Py_RETURN_NONE;
}

static PyMethodDef
b_py_answer_future_methods_[] = {
  {
    .ml_name = "add_callback",
    .ml_meth
      = (PyCFunction) b_py_answer_future_add_callback_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {},
};

static PyObject *
b_py_answer_future_get_state_(
    PyObject *self,
    void *opaque) {
  (void) opaque;
  struct B_PyAnswerFuture *future
    = b_py_answer_future(self);
  if (!future) {
    return NULL;
  }
  struct B_Error e;
  enum B_AnswerFutureState state;
  if (!b_answer_future_state(
      future->answer_future, &state, &e)) {
    b_py_raise(e);
    return NULL;
  }
  PyObject *value = Py_BuildValue("i", (int) state);
  if (!value) {
    return NULL;
  }
  return value;
}

static PyGetSetDef
b_py_answer_future_getset_[] = {
  {
    .name = "state",
    .get = b_py_answer_future_get_state_,
    .set = NULL,
    .doc = NULL,
    .closure = NULL,
  },
  {},
};

static struct B_PyIntConstant
b_py_answer_future_constants_[] = {
  {"FAILED",   B_FUTURE_FAILED},
  {"PENDING",  B_FUTURE_PENDING},
  {"RESOLVED", B_FUTURE_RESOLVED},
  {},
};

static PyTypeObject
b_py_answer_future_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyAnswerFuture),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_getset = b_py_answer_future_getset_,
  .tp_methods = b_py_answer_future_methods_,
  .tp_name = "_b.AnswerFuture",
};

B_WUR B_FUNC bool
b_py_answer_future_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_answer_future_type_) < 0) {
    return NULL;
  }
  if (!b_py_add_int_constants(
      b_py_answer_future_type_.tp_dict,
      b_py_answer_future_constants_)) {
    return NULL;
  }
  Py_INCREF(&b_py_answer_future_type_);
  if (PyModule_AddObject(
      module,
      "AnswerFuture",
      (PyObject *) &b_py_answer_future_type_)) {
    Py_DECREF(&b_py_answer_future_type_);
    return NULL;
  }
  return true;
}
