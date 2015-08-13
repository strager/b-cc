#include <B/Py/Private/Answer.h>
#include <B/Py/Private/Util.h>
#include <B/QuestionAnswer.h>

static bool
b_py_answer_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_answer_type) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyAnswer *
b_py_answer(
    B_BORROW PyObject *object) {
  if (!b_py_answer_check_(object)) {
    PyErr_SetString(PyExc_TypeError, "Expected Answer");
    return NULL;
  }
  return (struct B_PyAnswer *) object;
}

B_WUR B_FUNC B_OUT_BORROW struct B_IAnswer *
b_py_ianswer(
    B_BORROW struct B_PyAnswer *answer_py) {
  return (struct B_IAnswer *) answer_py;
}

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyAnswer *
b_py_answer_from_pointer(
    B_TRANSFER struct B_IAnswer *answer) {
  struct B_PyAnswer *answer_object
    = (struct B_PyAnswer *) answer;
  assert(b_py_answer_check_((PyObject *) answer_object));
  Py_INCREF(answer_object);
  return answer_object;
}

static PyMethodDef
b_py_answer_methods_[] = {
  B_PY_METHOD_DEF_END
};

PyTypeObject
b_py_answer_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyAnswer),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_BASETYPE | Py_TPFLAGS_DEFAULT,
  .tp_methods = b_py_answer_methods_,
  .tp_name = "_b.Answer",
};

B_WUR B_FUNC bool
b_py_answer_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_answer_type) < 0) {
    return false;
  }
  Py_INCREF(&b_py_answer_type);
  if (PyModule_AddObject(
      module,
      "Answer",
      (PyObject *) &b_py_answer_type)) {
    Py_DECREF(&b_py_answer_type);
    return false;
  }
  return true;
}
