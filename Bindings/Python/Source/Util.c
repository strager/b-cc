#include <B/Error.h>
#include <B/Py/Private/Util.h>

B_FUNC void
b_py_raise(
    struct B_Error error) {
  // TODO(strager)
  (void) error;
  __builtin_trap();
}

B_FUNC struct B_Error
b_py_error(
    void) {
  // TODO(strager)
  __builtin_trap();
}

B_FUNC struct B_Error
b_py_error_from(
    PyObject *object) {
  // TODO(strager)
  (void) object;
  return (struct B_Error) {.posix_error = EINVAL};
}

B_WUR B_FUNC B_OUT_TRANSFER PyObject *
b_py_to_utf8_string(
    B_BORROW PyObject *object) {
  if (PyString_Check(object)) {
    Py_INCREF(object);
    return object;
  }
  PyObject *unicode = PyUnicode_FromObject(object);
  if (!unicode) {
    return NULL;
  }
  PyObject *string = PyUnicode_AsEncodedString(
    unicode, "utf8", NULL);
  Py_DECREF(unicode);
  if (!string) {
    return NULL;
  }
  if (!PyString_Check(string)) {
    __builtin_trap();
  }
  return string;
}

B_WUR B_FUNC bool
b_py_add_int_constants(
    B_BORROW PyObject *dict,
    B_BORROW struct B_PyIntConstant const *constants) {
  for (
      struct B_PyIntConstant const *c = constants;
      c->name;
      ++c) {
    PyObject *value = Py_BuildValue("i", c->value);
    if (!value) {
      return false;
    }
    if (PyDict_SetItemString(dict, c->name, value) == -1) {
      Py_DECREF(value);
      return false;
    }
    Py_DECREF(value);
  }
  return true;
}
