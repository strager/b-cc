#include <B/Database.h>
#include <B/Error.h>
#include <B/Py/Private/Database.h>
#include <B/Py/Private/QuestionVTable.h>
#include <B/Py/Private/Util.h>

#include <sqlite3.h>

static PyTypeObject
b_py_database_type_;

static bool
b_py_database_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_database_type_) == 1;
}

B_WUR B_FUNC B_OUT_BORROW struct B_PyDatabase *
b_py_database(
    B_BORROW PyObject *object) {
  if (!b_py_database_check_(object)) {
    PyErr_SetString(PyExc_TypeError, "Expected Database");
    return NULL;
  }
  struct B_PyDatabase *db = (struct B_PyDatabase *) object;
  if (!db->database) {
    PyErr_SetString(
      PyExc_ValueError, "Database already closed");
    return NULL;
  }
  return db;
}

static PyObject *
b_py_database_open_sqlite3_(
    PyObject *cls,
    PyObject *args,
    PyObject *kwargs) {
  (void) cls;
  static char *keywords[]
    = {"sqlite_path", "sqlite_flags", "sqlite_vfs", NULL};
  char *sqlite_path;
  int sqlite_flags;
  PyObject *sqlite_vfs_object = Py_None;
  if (!PyArg_ParseTupleAndKeywords(
      args,
      kwargs,
      "es" "i" "|" "O",
      keywords,
      "utf8",
      &sqlite_path,
      &sqlite_flags,
      &sqlite_vfs_object)) {
    return NULL;
  }
  PyObject *sqlite_vfs_string = NULL;
  char const *sqlite_vfs;
  if (sqlite_vfs_object == Py_None) {
    sqlite_vfs = NULL;
  } else {
    sqlite_vfs_string
      = b_py_to_utf8_string(sqlite_vfs_object);
    sqlite_vfs = PyString_AS_STRING(sqlite_vfs_string);
  }
  PyObject *self = b_py_database_type_.tp_alloc(
    &b_py_database_type_, 0);
  if (!self) {
    __builtin_trap();
    return NULL;
  }
  struct B_PyDatabase *db_py = (struct B_PyDatabase *) self;
  struct B_Error e;
  if (!b_database_open_sqlite3(
      sqlite_path,
      sqlite_flags,
      sqlite_vfs,
      &db_py->database,
      &e)) {
    b_py_raise(e);
    __builtin_trap();
    return NULL;
  }
  PyMem_Free(sqlite_path);
  if (sqlite_vfs_string) {
    Py_DECREF(sqlite_vfs_string);
  }
  return self;
}

static PyObject *
b_py_database_close_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyDatabase *db_py = b_py_database(self);
  if (!db_py) {
    return NULL;
  }
  struct B_Error e;
  if (!b_database_close(db_py->database, &e)) {
    b_py_raise(e);
    return NULL;
  }
  db_py->database = NULL;
  Py_RETURN_NONE;
}

static PyObject *
b_py_database_check_all_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {"question_classes", NULL};
  PyObject *question_classes;
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "O", keywords, &question_classes)) {
    return NULL;
  }
  struct B_PyDatabase *db_py = b_py_database(self);
  if (!db_py) {
    return NULL;
  }
  struct B_QuestionVTable const **vtables;
  size_t vtable_count;
  if (PySequence_Check(question_classes)) {
    ssize_t length_signed
      = PySequence_Length(question_classes);
    if (length_signed == -1) {
      __builtin_trap();
    }
    if (length_signed < -1) {
      __builtin_trap();
    }
    vtable_count = (size_t) length_signed;
    // TODO(strager): Check for overflow.
    vtables = PyMem_Malloc(sizeof(*vtables) * vtable_count);
    if (!vtables) {
      __builtin_trap();
    }
    for (size_t i = 0; i < vtable_count; ++i) {
      PyObject *o = PySequence_GetItem(question_classes, i);
      if (!PyType_Check(o)) {
        PyErr_SetString(
          PyExc_TypeError, "Expected Question type");
        __builtin_trap();
      }
      struct B_QuestionVTable const *vtable
        = b_py_question_class_get_native_vtable(
          (PyTypeObject *) o);
      if (!vtable) {
        __builtin_trap();
      }
      vtables[i] = vtable;
      Py_DECREF(o);
    }
  } else if (PyIter_Check(question_classes)) {
    // TODO(strager)
    __builtin_trap();
  } else {
    PyErr_SetString(
      PyExc_TypeError,
      "question_classes must be iterable");
    return NULL;
  }
  struct B_Error e;
  if (!b_database_check_all(
      db_py->database, vtables, vtable_count, &e)) {
    PyMem_Free(vtables);
    b_py_raise(e);
    return NULL;
  }
  PyMem_Free(vtables);
  Py_RETURN_NONE;
}

static PyObject *
b_py_database_enter_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[] = {NULL};
  if (!PyArg_ParseTupleAndKeywords(
      args, kwargs, "", keywords)) {
    return NULL;
  }
  struct B_PyDatabase *db_py = b_py_database(self);
  if (!db_py) {
    return NULL;
  }
  Py_INCREF(db_py);
  return (PyObject *) db_py;
}

static PyObject *
b_py_database_exit_(
    PyObject *self,
    PyObject *args,
    PyObject *kwargs) {
  static char *keywords[]
    = {"type", "value", "traceback", NULL};
  PyObject *type;
  PyObject *value;
  PyObject *traceback;
  if (!PyArg_ParseTupleAndKeywords(
      args,
      kwargs,
      "OOO",
      keywords,
      &type,
      &value,
      &traceback)) {
    return NULL;
  }
  struct B_PyDatabase *db_py = b_py_database(self);
  if (!db_py) {
    return NULL;
  }
  PyObject *ret = PyObject_CallMethod(
    (PyObject *) db_py, "close", NULL);
  if (!ret) {
    return NULL;
  }
  Py_DECREF(ret);
  Py_RETURN_NONE;
}

static PyMethodDef
b_py_database_methods_[] = {
  {
    .ml_name = "open_sqlite3",
    .ml_meth = (PyCFunction) b_py_database_open_sqlite3_,
    .ml_flags = METH_CLASS | METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "close",
    .ml_meth = (PyCFunction) b_py_database_close_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "check_all",
    .ml_meth = (PyCFunction) b_py_database_check_all_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "__enter__",
    .ml_meth = (PyCFunction) b_py_database_enter_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  {
    .ml_name = "__exit__",
    .ml_meth = (PyCFunction) b_py_database_exit_,
    .ml_flags = METH_KEYWORDS,
    .ml_doc = "",
  },
  B_PY_METHOD_DEF_END
};

static struct B_PyIntConstant
b_py_database_constants_[] = {
  {"SQLITE_OPEN_CREATE",       SQLITE_OPEN_CREATE},
  {"SQLITE_OPEN_FULLMUTEX",    SQLITE_OPEN_FULLMUTEX},
  {"SQLITE_OPEN_MEMORY",       SQLITE_OPEN_MEMORY},
  {"SQLITE_OPEN_NOMUTEX",      SQLITE_OPEN_NOMUTEX},
  {"SQLITE_OPEN_PRIVATECACHE", SQLITE_OPEN_PRIVATECACHE},
  {"SQLITE_OPEN_READONLY",     SQLITE_OPEN_READONLY},
  {"SQLITE_OPEN_READWRITE",    SQLITE_OPEN_READWRITE},
  {"SQLITE_OPEN_SHAREDCACHE",  SQLITE_OPEN_SHAREDCACHE},
  {"SQLITE_OPEN_URI",          SQLITE_OPEN_URI},
  B_PY_INT_CONSTANT_END
};

static PyTypeObject
b_py_database_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyDatabase),
  .tp_dealloc = NULL,  // TODO(strager)
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_methods = b_py_database_methods_,
  .tp_name = "_b.Database",
};

B_WUR B_FUNC bool
b_py_database_init(
    B_BORROW PyObject *module) {
  if (PyType_Ready(&b_py_database_type_) < 0) {
    return false;
  }
  if (!b_py_add_int_constants(
      b_py_database_type_.tp_dict,
      b_py_database_constants_)) {
    return false;
  }
  Py_INCREF(&b_py_database_type_);
  if (PyModule_AddObject(
      module,
      "Database",
      (PyObject *) &b_py_database_type_)) {
    Py_DECREF(&b_py_database_type_);
    return false;
  }
  return true;
}
