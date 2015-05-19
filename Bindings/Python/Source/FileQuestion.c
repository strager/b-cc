#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/Py/Private/FileQuestion.h>
#include <B/Py/Private/Question.h>
#include <B/Py/Private/QuestionVTable.h>
#include <B/Py/Private/Util.h>

struct B_PyFileQuestion {
  struct B_PyQuestion super;
};

static PyTypeObject
b_py_file_question_type_;

static bool
b_py_file_question_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_file_question_type_) == 1;
}

static B_OUT_TRANSFER struct B_PyFileQuestion *
b_py_file_question_(
    B_BORROW PyObject *object) {
  if (!b_py_file_question_check_(object)) {
    PyErr_SetString(
      PyExc_TypeError, "Expected FileQuestion");
    return NULL;
  }
  return (struct B_PyFileQuestion *) object;
}

static PyObject *
b_py_file_question_new_(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs) {
  assert(type == &b_py_file_question_type_);
  static char *keywords[]
    = {"path", NULL};
  char *path;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "es", keywords, "utf8", &path)) {
    return NULL;
  }
  PyObject *self = b_py_file_question_type_.tp_alloc(
    &b_py_file_question_type_, 0);
  if (!self) {
    PyMem_Free(path);
    return NULL;
  }
  struct B_PyFileQuestion *file_question_py
    = (struct B_PyFileQuestion *) self;
  struct B_Error e;
  if (!b_file_question_allocate(
      path, &file_question_py->super.question, &e)) {
    PyMem_Free(path);
    b_py_raise(e);
    return NULL;
  }
  return self;
}

static PyObject *
b_py_file_question_get_path_(
    PyObject *self,
    void *opaque) {
  (void) opaque;
  struct B_PyFileQuestion *file_question_py
    = b_py_file_question_(self);
  if (!file_question_py) {
    return NULL;
  }
  struct B_Error e;
  char const *path;
  if (!b_file_question_path(
      file_question_py->super.question, &path, &e)) {
    b_py_raise(e);
    return NULL;
  }
  // FIXME(strager): unicode?
  PyObject *path_object = PyString_FromString(path);
  if (!path_object) {
    return NULL;
  }
  return path_object;
}

static PyGetSetDef
b_py_file_question_getset_[] = {
  {
    .name = "path",
    .get = b_py_file_question_get_path_,
    .set = NULL,
    .doc = NULL,
    .closure = NULL,
  },
  B_PY_GET_SET_DEF_END
};

static PyTypeObject
b_py_file_question_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_base = &b_py_question_type,
  .tp_basicsize = sizeof(struct B_PyFileQuestion),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_getset = b_py_file_question_getset_,
  .tp_name = "_b.FileQuestion",
  .tp_new = b_py_file_question_new_,
};

B_WUR B_FUNC bool
b_py_file_question_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_file_question_type_) < 0) {
    return false;
  }
  if (!b_py_question_class_set_native_vtable(
      &b_py_file_question_type_,
      b_file_question_vtable())) {
    return false;
  }
  Py_INCREF(&b_py_file_question_type_);
  if (PyModule_AddObject(
      module,
      "FileQuestion",
      (PyObject *) &b_py_file_question_type_)) {
    Py_DECREF(&b_py_file_question_type_);
    return false;
  }
  return true;
}
