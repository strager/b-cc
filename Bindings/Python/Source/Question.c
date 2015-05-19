#include <B/Error.h>
#include <B/Py/Private/Question.h>
#include <B/Py/Private/QuestionVTable.h>
#include <B/Py/Private/Util.h>
#include <B/QuestionAnswer.h>

static bool
b_py_question_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_question_type) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyQuestion *
b_py_question(
    B_BORROW PyObject *object) {
  if (!b_py_question_check_(object)) {
    PyErr_SetString(PyExc_TypeError, "Expected Question");
    return NULL;
  }
  return (struct B_PyQuestion *) object;
}

B_WUR B_FUNC B_OUT_BORROW struct B_IQuestion *
b_py_iquestion(
    B_BORROW struct B_PyQuestion *question_py) {
  return (struct B_IQuestion *) question_py;
}

B_WUR B_FUNC B_BORROW struct B_QuestionVTable const *
b_py_question_python_vtable(
    B_BORROW struct B_PyQuestion *question_py) {
  return b_py_question_class_get_python_vtable(
    Py_TYPE(question_py));
}

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyQuestion *
b_py_question_from_pointer(
    B_TRANSFER struct B_IQuestion *question) {
  struct B_PyQuestion *question_object
    = (struct B_PyQuestion *) question;
  assert(b_py_question_check_(
    (PyObject *) question_object));
  Py_INCREF(question_object);
  return question_object;
}

static PyObject *
b_py_question_query_answer_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyQuestion *q_py = b_py_question(self);
  if (!q_py) {
    return NULL;
  }
  __builtin_trap();
}

static PyObject *
b_py_question_serialize_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyQuestion *q_py = b_py_question(self);
  if (!q_py) {
    return NULL;
  }
  __builtin_trap();
}

static PyObject *
b_py_question_deserialize_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyQuestion *q_py = b_py_question(self);
  if (!q_py) {
    return NULL;
  }
  __builtin_trap();
}

static PyObject *
b_py_question_uuid_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyQuestion *q_py = b_py_question(self);
  if (!q_py) {
    return NULL;
  }
  __builtin_trap();
}

static PyMethodDef
b_py_question_methods_[] = {
  {
    .ml_name = "query_answer",
    .ml_meth = (PyCFunction) b_py_question_query_answer_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "serialize",
    .ml_meth = (PyCFunction) b_py_question_serialize_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "deserialize",
    .ml_meth = (PyCFunction) b_py_question_deserialize_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "uuid",
    .ml_meth = (PyCFunction) b_py_question_uuid_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  B_PY_METHOD_DEF_END
};

PyTypeObject
b_py_question_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyQuestion),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = b_py_question_methods_,
  .tp_name = "_b.Question",
};

B_WUR B_FUNC bool
b_py_question_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_question_type) < 0) {
    return false;
  }
  Py_INCREF(&b_py_question_type);
  if (PyModule_AddObject(
      module,
      "Question",
      (PyObject *) &b_py_question_type)) {
    Py_DECREF(&b_py_question_type);
    return false;
  }
  return true;
}
