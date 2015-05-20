#include <B/Error.h>
#include <B/Process.h>
#include <B/Py/Private/Process.h>
#include <B/Py/Private/Util.h>

#include <structmember.h>

struct B_PyProcessExitStatus {
  PyObject_HEAD
  struct B_ProcessExitStatus status;
};

static PyTypeObject
b_py_process_exit_status_type_;

static PyTypeObject
b_py_process_exit_status_signal_type_;

static PyTypeObject
b_py_process_exit_status_exception_type_;

static PyTypeObject
b_py_process_exit_status_code_type_;

static bool
b_py_process_exit_status_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object,
    (PyObject *) &b_py_process_exit_status_type_) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyProcessExitStatus *
b_py_process_exit_status(
    B_BORROW PyObject *object) {
  if (!b_py_process_exit_status_check_(object)) {
    PyErr_SetString(
      PyExc_TypeError, "Expected ProcessExitStatus");
    return NULL;
  }
  return (struct B_PyProcessExitStatus *) object;
}

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyProcessExitStatus *
b_py_process_exit_status_from_pointer(
    B_TRANSFER struct B_ProcessExitStatus const *status) {
  PyTypeObject *class;
  switch (status->type) {
  case B_PROCESS_EXIT_STATUS_SIGNAL:
    class = &b_py_process_exit_status_signal_type_;
    break;
  case B_PROCESS_EXIT_STATUS_CODE:
    class = &b_py_process_exit_status_code_type_;
    break;
  case B_PROCESS_EXIT_STATUS_EXCEPTION:
    class = &b_py_process_exit_status_exception_type_;
    break;
  default:
    PyErr_SetString(
      PyExc_ValueError,
      "Bad native ProcessExitStatus type");
    return NULL;
  }
  PyObject *self = class->tp_alloc(class, 0);
  if (!self) {
    return NULL;
  }
  struct B_PyProcessExitStatus *status_py
    = (struct B_PyProcessExitStatus *) self;
  status_py->status = *status;
  return status_py;
}

static PyObject *
b_py_process_exit_status_richcompare_(
    PyObject *a,
    PyObject *b,
    int op) {
  struct B_PyProcessExitStatus *a_py
    = b_py_process_exit_status(a);
  if (!a_py) {
    return NULL;
  }
  struct B_PyProcessExitStatus *b_py
    = b_py_process_exit_status(b);
  if (!b_py) {
    return NULL;
  }
  switch (op) {
  case Py_EQ:
    if (b_process_exit_status_equal(
        &a_py->status, &b_py->status)) {
      Py_RETURN_TRUE;
    } else {
      Py_RETURN_FALSE;
    }
  case Py_NE:
    if (b_process_exit_status_equal(
        &a_py->status, &b_py->status)) {
      Py_RETURN_FALSE;
    } else {
      Py_RETURN_TRUE;
    }
  default:
    Py_INCREF(Py_NotImplemented);
    return Py_NotImplemented;
  }
}

static PyTypeObject
b_py_process_exit_status_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyProcessExitStatus),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_name = "_b.ProcessExitStatus",
  .tp_richcompare = b_py_process_exit_status_richcompare_,
};

static PyMemberDef
b_py_process_exit_status_signal_members_[] = {
  {
    .doc = "", // TODO
    .flags = READONLY,
    .name = "signal_number",
    .offset = offsetof(
        struct B_PyProcessExitStatus,
        status.u.signal.signal_number),
    .type = T_INT,
  },
  B_PY_MEMBER_DEF_END
};

static PyObject *
b_py_process_exit_status_signal_new_(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs) {
  (void) type;
  static char *keywords[] = {"signal_number", NULL};
  int signal_number;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "i", keywords, &signal_number)) {
    return NULL;
  }
  struct B_ProcessExitStatus status = {
    .type = B_PROCESS_EXIT_STATUS_SIGNAL,
    .u = {.signal = {.signal_number = signal_number}},
  };
  return (PyObject *)
    b_py_process_exit_status_from_pointer(&status);
}

static PyTypeObject
b_py_process_exit_status_signal_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_base = &b_py_process_exit_status_type_,
  .tp_basicsize = sizeof(struct B_PyProcessExitStatus),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_members = b_py_process_exit_status_signal_members_,
  .tp_name = "_b.ProcessExitStatusSignal",
  .tp_new = b_py_process_exit_status_signal_new_,
};

static PyMemberDef
b_py_process_exit_status_exception_members_[] = {
  {
    .doc = "", // TODO
    .flags = READONLY,
    .name = "exception_code",
    .offset = offsetof(
        struct B_PyProcessExitStatus,
        status.u.exception.code),
    .type = T_UINT,  // FIXME(strager)
  },
  B_PY_MEMBER_DEF_END
};

static PyObject *
b_py_process_exit_status_exception_new_(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs) {
  (void) type;
  static char *keywords[] = {"exception_code", NULL};
  if (sizeof(PY_LONG_LONG) < sizeof(uint32_t)) {
    // TODO(strager): static_assert.
    PyErr_SetString(
      PyExc_TypeError,
      "C long long is smaller than C uint32_t");
    return NULL;
  }
  PY_LONG_LONG exception_code;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "L", keywords, &exception_code)) {
    return NULL;
  }
  struct B_ProcessExitStatus status = {
    .type = B_PROCESS_EXIT_STATUS_EXCEPTION,
    // FIXME(strager): What about truncation?
    .u = {.exception = {.code = (uint32_t) exception_code}},
  };
  return (PyObject *)
    b_py_process_exit_status_from_pointer(&status);
}

static PyTypeObject
b_py_process_exit_status_exception_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_base = &b_py_process_exit_status_type_,
  .tp_basicsize = sizeof(struct B_PyProcessExitStatus),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_members = b_py_process_exit_status_exception_members_,
  .tp_name = "_b.ProcessExitStatusException",
  .tp_new = b_py_process_exit_status_exception_new_,
};

static PyMemberDef
b_py_process_exit_status_code_members_[] = {
  {
    .doc = "", // TODO
    .flags = READONLY,
    .name = "exit_code",
    .offset = offsetof(
        struct B_PyProcessExitStatus,
        status.u.code.exit_code),
    .type = T_ULONGLONG,  // FIXME(strager)
  },
  B_PY_MEMBER_DEF_END
};

static PyObject *
b_py_process_exit_status_code_new_(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwargs) {
  (void) type;
  static char *keywords[] = {"exit_code", NULL};
  if (sizeof(PY_LONG_LONG) < sizeof(int64_t)) {
    // TODO(strager): static_assert.
    PyErr_SetString(
      PyExc_TypeError,
      "C long long is smaller than C int64_t");
    return NULL;
  }
  PY_LONG_LONG exit_code;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "L", keywords, &exit_code)) {
    return NULL;
  }
  struct B_ProcessExitStatus status = {
    .type = B_PROCESS_EXIT_STATUS_CODE,
    // FIXME(strager): What about truncation?
    .u = {.code = {.exit_code = exit_code}},
  };
  return (PyObject *)
    b_py_process_exit_status_from_pointer(&status);
}

static PyTypeObject
b_py_process_exit_status_code_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_base = &b_py_process_exit_status_type_,
  .tp_basicsize = sizeof(struct B_PyProcessExitStatus),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_members = b_py_process_exit_status_code_members_,
  .tp_name = "_b.ProcessExitStatusCode",
  .tp_new = b_py_process_exit_status_code_new_,
};

B_WUR B_FUNC bool
b_py_process_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_process_exit_status_type_) < 0) {
    return NULL;
  }
  Py_INCREF(&b_py_process_exit_status_type_);
  if (PyModule_AddObject(
      module,
      "ProcessExitStatus",
      (PyObject *) &b_py_process_exit_status_type_)) {
    Py_DECREF(&b_py_process_exit_status_type_);
    return NULL;
  }

  if (PyType_Ready(
      &b_py_process_exit_status_signal_type_) < 0) {
    return NULL;
  }
  Py_INCREF(&b_py_process_exit_status_signal_type_);
  if (PyModule_AddObject(
      module,
      "ProcessExitStatusSignal",
      (PyObject *)
        &b_py_process_exit_status_signal_type_)) {
    Py_DECREF(&b_py_process_exit_status_signal_type_);
    return NULL;
  }

  if (PyType_Ready(
      &b_py_process_exit_status_exception_type_) < 0) {
    return NULL;
  }
  Py_INCREF(&b_py_process_exit_status_exception_type_);
  if (PyModule_AddObject(
      module,
      "ProcessExitStatusException",
      (PyObject *)
        &b_py_process_exit_status_exception_type_)) {
    Py_DECREF(&b_py_process_exit_status_exception_type_);
    return NULL;
  }

  if (PyType_Ready(
      &b_py_process_exit_status_code_type_) < 0) {
    return NULL;
  }
  Py_INCREF(&b_py_process_exit_status_code_type_);
  if (PyModule_AddObject(
      module,
      "ProcessExitStatusCode",
      (PyObject *) &b_py_process_exit_status_code_type_)) {
    Py_DECREF(&b_py_process_exit_status_code_type_);
    return NULL;
  }

  return true;
}
