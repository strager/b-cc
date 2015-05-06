#include <B/AnswerFuture.h>
#include <B/Error.h>
#include <B/Main.h>
#include <B/Py/Private/AnswerContext.h>
#include <B/Py/Private/AnswerFuture.h>
#include <B/Py/Private/Database.h>
#include <B/Py/Private/Main.h>
#include <B/Py/Private/Question.h>
#include <B/Py/Private/RunLoop.h>
#include <B/Py/Private/Util.h>

static PyTypeObject
b_py_main_type_;

static bool
b_py_main_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_main_type_) == 1;
}

static B_OUT_TRANSFER struct B_PyMain *
b_py_main_(
    B_BORROW PyObject *object) {
  if (!b_py_main_check_(object)) {
    PyErr_SetString(PyExc_TypeError, "Expected Main");
    return NULL;
  }
  return (struct B_PyMain *) object;
}

static B_FUNC bool
b_py_main_callback_(
    B_BORROW void *opaque,
    B_BORROW struct B_Main *main,
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  struct B_PyMain *main_py = opaque;
  assert(main_py->main == main);
  if (!PyCallable_Check(main_py->callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    *e = b_py_error();
    return false;
  }
  struct B_PyAnswerContext *ac_py
    = b_py_answer_context_from_pointer(ac, main_py);
  if (!ac_py) {
    *e = b_py_error();
    return false;
  }
  PyObject *args = Py_BuildValue(
    "(OO)", (PyObject *) main_py, (PyObject *) ac_py);
  Py_DECREF(ac_py);
  if (!args) {
    *e = b_py_error();
    return false;
  }
  PyObject *result = PyEval_CallObject(
    main_py->callback, args);
  Py_DECREF(args);
  if (!result) {
    *e = b_py_error();
    return false;
  }
  return true;
}

static PyObject *
b_py_main_new_(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs) {
  assert(type == &b_py_main_type_);
  static char *keywords[]
    = {"database", "run_loop", "callback", NULL};
  PyObject *database;
  PyObject *run_loop;
  PyObject *callback;
  if (!PyArg_ParseTupleAndKeywords(
      args,
      kwargs,
      "OOO",
      keywords,
      &database,
      &run_loop,
      &callback)) {
    return NULL;
  }
  struct B_PyDatabase *db_py = b_py_database(database);
  if (!db_py) {
    return NULL;
  }
  struct B_PyRunLoop *rl_py = b_py_run_loop(run_loop);
  if (!rl_py) {
    return NULL;
  }
  if (!PyCallable_Check(callback)) {
    PyErr_SetString(
      PyExc_TypeError, "callback must be callable");
    return NULL;
  }
  PyObject *self = b_py_main_type_.tp_alloc(
    &b_py_main_type_, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyMain *main_py = (struct B_PyMain *) self;
  struct B_Error e;
  if (!b_main_allocate(
      db_py->database,
      rl_py->run_loop,
      b_py_main_callback_,
      main_py,
      &main_py->main,
      &e)) {
    b_py_raise(e);
    __builtin_trap();
    return NULL;
  }
  main_py->database = database;
  Py_INCREF(database);
  main_py->run_loop = run_loop;
  Py_INCREF(run_loop);
  main_py->callback = callback;
  Py_INCREF(callback);
  return self;
}

static PyObject *
b_py_main_answer_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {"question", NULL};
  PyObject *question_object;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "O", keywords, &question_object)) {
    return NULL;
  }
  struct B_PyQuestion *question_py
    = b_py_question(question_object);
  if (!question_py) {
    return NULL;
  }
  struct B_PyMain *main_py = b_py_main_(self);
  if (!main_py) {
    return NULL;
  }
  struct B_QuestionVTable const *vtable
    = b_py_question_python_vtable(question_py);
  if (!vtable) {
    return NULL;
  }
  struct B_Error e;
  struct B_AnswerFuture *future;
  if (!b_main_answer(
      main_py->main,
      b_py_iquestion(question_py),
      vtable,
      &future,
      &e)) {
    b_py_raise(e);
    return NULL;
  }
  struct B_PyAnswerFuture *future_py
    = b_py_answer_future_from_pointer(future);
  if (!future_py) {
    b_answer_future_release(future);
    return NULL;
  }
  return (PyObject *) future_py;
}

static PyMethodDef
b_py_main_methods_[] = {
  {
    .ml_name = "answer",
    .ml_meth = (PyCFunction) b_py_main_answer_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {},
};

static PyTypeObject
b_py_main_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyMain),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = b_py_main_methods_,
  .tp_name = "_b.Main",
  .tp_new = b_py_main_new_,
};

B_WUR B_FUNC bool
b_py_main_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_main_type_) < 0) {
    return false;
  }
  Py_INCREF(&b_py_main_type_);
  if (PyModule_AddObject(
      module, "Main", (PyObject *) &b_py_main_type_)) {
    Py_DECREF(&b_py_main_type_);
    return false;
  }
  return true;
}
