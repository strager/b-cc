#include <B/AnswerContext.h>
#include <B/AnswerFuture.h>
#include <B/Error.h>
#include <B/Py/Private/AnswerContext.h>
#include <B/Py/Private/AnswerFuture.h>
#include <B/Py/Private/Question.h>
#include <B/Py/Private/Util.h>
#include <B/QuestionAnswer.h>

static PyTypeObject
b_py_answer_context_type_;

static bool
b_py_answer_context_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_answer_context_type_) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyAnswerContext *
b_py_answer_context(
    B_BORROW PyObject *object) {
  if (!b_py_answer_context_check_(object)) {
    PyErr_SetString(
      PyExc_TypeError, "Expected AnswerContext");
    return NULL;
  }
  struct B_PyAnswerContext *ac_py
    = (struct B_PyAnswerContext *) object;
  if (!ac_py->answer_context) {
    PyErr_SetString(
      PyExc_ValueError, "AnswerContext already closed");
    return NULL;
  }
  return ac_py;
}

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyAnswerContext *
b_py_answer_context_from_pointer(
    B_TRANSFER struct B_AnswerContext *answer_context,
    B_BORROW struct B_PyMain *main_py) {
  PyObject *self = b_py_answer_context_type_.tp_alloc(
    &b_py_answer_context_type_, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyAnswerContext *ac_py
    = (struct B_PyAnswerContext *) self;
  ac_py->answer_context = answer_context;
  ac_py->main = (PyObject *) main_py;
  Py_INCREF(ac_py->main);
  return ac_py;
}

static PyObject *
b_py_answer_context_need_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {"questions", NULL};
  PyObject *questions_object;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "O", keywords, &questions_object)) {
    return NULL;
  }
  struct B_PyAnswerContext *ac_py
    = b_py_answer_context(self);
  if (!ac_py) {
    return NULL;
  }
  size_t question_count;
  struct B_IQuestion **questions;
  struct B_QuestionVTable const **vtables;
  if (PySequence_Check(questions_object)) {
    ssize_t length_signed
      = PySequence_Length(questions_object);
    if (length_signed == -1) {
      __builtin_trap();
    }
    if (length_signed < -1) {
      __builtin_trap();
    }
    question_count = length_signed;
    // TODO(strager): Check for overflow.
    questions = PyMem_Malloc(
      sizeof(*questions) * question_count);
    if (!questions) {
      __builtin_trap();
    }
    // TODO(strager): Check for overflow.
    vtables = PyMem_Malloc(
      sizeof(*vtables) * question_count);
    if (!vtables) {
      __builtin_trap();
    }
    for (size_t i = 0; i < question_count; ++i) {
      PyObject *o = PySequence_GetItem(questions_object, i);
      struct B_PyQuestion *q_py = b_py_question(o);
      if (!q_py) {
        __builtin_trap();
        return NULL;
      }
      struct B_QuestionVTable const *vtable
        = b_py_question_python_vtable(q_py);
      if (!vtable) {
        __builtin_trap();
        return NULL;
      }
      struct B_Error e;
      if (!vtable->replicate(
          b_py_iquestion(q_py), &questions[i], &e)) {
        __builtin_trap();
        b_py_raise(e);
        return NULL;
      }
      vtables[i] = vtable;
      Py_DECREF(o);
    }
  } else if (PyIter_Check(questions_object)) {
    // TODO(strager)
    __builtin_trap();
  } else {
    PyErr_SetString(
      PyExc_TypeError, "questions must be iterable");
    return NULL;
  }
  struct B_Error e;
  struct B_AnswerFuture *future;
  if (!b_answer_context_need(
      ac_py->answer_context,
      questions,
      vtables,
      question_count,
      &future,
      &e)) {
    __builtin_trap();
    b_py_raise(e);
    return NULL;
  }
  struct B_PyAnswerFuture *future_py
    = b_py_answer_future_from_pointer(future);
  if (!future_py) {
    b_answer_future_release(future);
    __builtin_trap();
    return NULL;
  }
  PyMem_Free(questions);
  PyMem_Free(vtables);
  return (PyObject *) future_py;
}

static PyObject *
b_py_answer_context_succeed_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyAnswerContext *ac_py
    = b_py_answer_context(self);
  if (!ac_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_answer_context_succeed(
      ac_py->answer_context, &e)) {
    b_py_raise(e);
    return NULL;
  }
  ac_py->answer_context = NULL;
  Py_RETURN_NONE;
}

static PyObject *
b_py_answer_context_fail_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {"error", NULL};
  PyObject *error = Py_None;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "|O", keywords, &error)) {
    return NULL;
  }
  struct B_PyAnswerContext *ac_py
    = b_py_answer_context(self);
  if (!ac_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_answer_context_fail(
      ac_py->answer_context, b_py_error_from(error), &e)) {
    b_py_raise(e);
    return NULL;
  }
  ac_py->answer_context = NULL;
  Py_RETURN_NONE;
}

static PyMethodDef
b_py_answer_context_methods_[] = {
  {
    .ml_name = "need",
    .ml_meth = (PyCFunction) b_py_answer_context_need_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "succeed",
    .ml_meth = (PyCFunction) b_py_answer_context_succeed_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "fail",
    .ml_meth = (PyCFunction) b_py_answer_context_fail_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {},
};

static PyObject *
b_py_answer_context_get_question_(
    PyObject *self,
    void *opaque) {
  (void) opaque;
  struct B_PyAnswerContext *ac_py
    = b_py_answer_context(self);
  if (!ac_py) {
    return NULL;
  }
  struct B_Error e;
  struct B_IQuestion *q;
  struct B_QuestionVTable const *vtable;
  if (!b_answer_context_question(
      ac_py->answer_context, &q, &vtable, &e)) {
    b_py_raise(e);
    return NULL;
  }
  struct B_IQuestion *q_replica;
  if (!vtable->replicate(q, &q_replica, &e)) {
    b_py_raise(e);
    return NULL;
  }
  struct B_PyQuestion *q_py
    = b_py_question_from_pointer(q_replica);
  if (!q_py) {
    vtable->deallocate(q_replica);
    return NULL;
  }
  return (PyObject *) q_py;
}

static PyGetSetDef
b_py_answer_context_getset_[] = {
  {
    .name = "question",
    .get = b_py_answer_context_get_question_,
    .set = NULL,
    .doc = NULL,
    .closure = NULL,
  },
  {},
};

static PyTypeObject
b_py_answer_context_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyAnswerContext),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_getset = b_py_answer_context_getset_,
  .tp_methods = b_py_answer_context_methods_,
  .tp_name = "_b.AnswerContext",
};

B_WUR B_FUNC bool
b_py_answer_context_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_answer_context_type_) < 0) {
    return false;
  }
  Py_INCREF(&b_py_answer_context_type_);
  if (PyModule_AddObject(
      module,
      "AnswerContext",
      (PyObject *) &b_py_answer_context_type_)) {
    Py_DECREF(&b_py_answer_context_type_);
    return false;
  }
  return true;
}
