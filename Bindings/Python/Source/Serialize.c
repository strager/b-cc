#include <B/Error.h>
#include <B/Py/Private/Serialize.h>
#include <B/Py/Private/Util.h>
#include <B/Serialize.h>

struct B_PyByteSink {
  PyObject_HEAD
  struct B_ByteSink *byte_sink;
};

static PyTypeObject
b_py_byte_sink_type_;

static bool
b_py_byte_sink_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_byte_sink_type_) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyByteSink *
b_py_byte_sink(
    B_BORROW PyObject *object) {
  if (!b_py_byte_sink_check_(object)) {
    PyErr_SetString(
      PyExc_TypeError, "Expected ByteSink");
    return NULL;
  }
  return (struct B_PyByteSink *) object;
}

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyByteSink *
b_py_byte_sink_from_pointer(
    B_TRANSFER struct B_ByteSink *byte_sink) {
  PyObject *self = b_py_byte_sink_type_.tp_alloc(
    &b_py_byte_sink_type_, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyByteSink *sink_py
    = (struct B_PyByteSink *) self;
  sink_py->byte_sink = byte_sink;
  return sink_py;
}

static PyObject *
b_py_byte_sink_serialize_data_and_size_8_be_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {"data", NULL};
  char const *data;
  Py_ssize_t data_size;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "s#", keywords, &data, &data_size)) {
    return NULL;
  }
  struct B_PyByteSink *sink_py = b_py_byte_sink(self);
  if (!sink_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_serialize_data_and_size_8_be(
      sink_py->byte_sink,
      (uint8_t const *) data,
      (size_t) data_size,
      &e)) {
    b_py_raise(e);
    return NULL;
  }
  Py_RETURN_NONE;
}

static PyMethodDef
b_py_byte_sink_methods_[] = {
  {
    .ml_name = "serialize_data_and_size_8_be",
    .ml_meth = (PyCFunction)
      b_py_byte_sink_serialize_data_and_size_8_be_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  B_PY_METHOD_DEF_END
};

static PyTypeObject
b_py_byte_sink_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyByteSink),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = b_py_byte_sink_methods_,
  .tp_name = "_b.ByteSink",
};

B_WUR B_FUNC bool
b_py_byte_sink_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_byte_sink_type_) < 0) {
    return false;
  }
  Py_INCREF(&b_py_byte_sink_type_);
  if (PyModule_AddObject(
      module,
      "ByteSink",
      (PyObject *) &b_py_byte_sink_type_)) {
    Py_DECREF(&b_py_byte_sink_type_);
    return false;
  }
  return true;
}
