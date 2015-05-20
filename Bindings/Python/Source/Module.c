#include <B/Py/Private/AnswerContext.h>
#include <B/Py/Private/AnswerFuture.h>
#include <B/Py/Private/Database.h>
#include <B/Py/Private/FileQuestion.h>
#include <B/Py/Private/Main.h>
#include <B/Py/Private/Process.h>
#include <B/Py/Private/Question.h>
#include <B/Py/Private/QuestionVTable.h>
#include <B/Py/Private/RunLoop.h>

#include <Python.h>

static bool
b_init_(
    void) {
  PyObject *module = Py_InitModule3("_b", NULL, "");
  if (!module) {
    return false;
  }

  // Must be before b_py_file_question_init.
  if (!b_py_question_vtable_init()) {
    return false;
  }

  // Must be before b_py_file_question_init.
  if (!b_py_question_init(module)) {
    return false;
  }

  if (!b_py_answer_context_init(module)) {
    return false;
  }
  if (!b_py_answer_future_init(module)) {
    return false;
  }
  if (!b_py_database_init(module)) {
    return false;
  }
  if (!b_py_file_question_init(module)) {
    return false;
  }
  if (!b_py_main_init(module)) {
    return false;
  }
  if (!b_py_process_init(module)) {
    return false;
  }
  if (!b_py_run_loop_init(module)) {
    return false;
  }
  return true;
}

PyMODINIT_FUNC
init_b(
    void);

PyMODINIT_FUNC
init_b(
    void) {
  PyErr_SetString(PyExc_TypeError, "hello");
  if (b_init_()) {
    // Clear any exceptions, since Python checks import
    // success or failure based on the presense of a current
    // exception.
    PyErr_Clear();
  } else {
    // Propagate the exception into the caller of import.
    assert(PyErr_Occurred());
  }
}
