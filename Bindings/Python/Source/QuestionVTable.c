#include <B/Error.h>
#include <B/Py/Private/Question.h>
#include <B/Py/Private/QuestionVTable.h>
#include <B/Py/Private/Util.h>
#include <B/QuestionAnswer.h>

struct B_PyQuestionVTable {
  PyObject_HEAD
  struct B_QuestionVTable const *native_vtable;
  struct B_QuestionVTable python_vtable;
  // TODO(strager): AnswerVTable.
};

static PyTypeObject
b_py_question_vtable_type_;

static bool
b_py_question_vtable_check_(
    B_BORROW PyObject *object) {
  return PyObject_IsInstance(
    object, (PyObject *) &b_py_question_vtable_type_) == 1;
}

static B_WUR B_FUNC bool
b_py_question_and_vtable_(
    B_BORROW struct B_IQuestion const *question,
    B_OUT_BORROW struct B_IQuestion **out_question,
    B_OUT_BORROW struct B_QuestionVTable const **out_vtable,
    B_OUT struct B_Error *e) {
  struct B_PyQuestion *question_py
    = b_py_question_from_pointer(
      (struct B_IQuestion *) question);
  if (!question_py) {
    *e = b_py_error();
    return false;
  }
  struct B_QuestionVTable const *vtable
    = b_py_question_class_get_native_vtable(
      Py_TYPE(question_py));
  if (!vtable) {
    Py_DECREF(question_py);
    *e = b_py_error();
    return false;
  }
  *out_question = question_py->question;
  *out_vtable = vtable;
  // HACK(strager): We know that question_py effectively
  // returns an object we can safely -1 and keep alive. This
  // Py_DECREF is thus safe.
  Py_DECREF(question_py);
  return true;
}

static B_WUR B_FUNC bool
b_py_question_query_answer_(
    B_BORROW struct B_IQuestion const *question,
    B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **out,
    B_OUT struct B_Error *e) {
  struct B_IQuestion *native_question;
  struct B_QuestionVTable const *native_vtable;
  if (!b_py_question_and_vtable_(
      question, &native_question, &native_vtable, e)) {
    return false;
  }
  struct B_IAnswer *answer;
  if (!native_vtable->query_answer(
      native_question, &answer, e)) {
    return false;
  }
  *out = answer;
  return true;
}

static B_WUR B_FUNC void
b_py_question_deallocate_(
    B_TRANSFER struct B_IQuestion *question) {
  struct B_PyQuestion *question_py
    = b_py_question_from_pointer(
      (struct B_IQuestion *) question);
  if (!question_py) {
    __builtin_trap();
    return;
  }
  Py_DECREF(question_py);
  return;
}

static B_WUR B_FUNC bool
b_py_question_replicate_(
    B_BORROW struct B_IQuestion const *question,
    B_OUT_TRANSFER struct B_IQuestion **out,
    struct B_Error *e) {
  struct B_PyQuestion *question_py
    = b_py_question_from_pointer(
      (struct B_IQuestion *) question);
  if (!question_py) {
    *e = b_py_error();
    return false;
  }
  Py_INCREF(question_py);
  *out = b_py_iquestion(question_py);
  return true;
}

static B_WUR B_FUNC bool
b_py_question_serialize_(
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_ByteSink *byte_sink,
    B_OUT struct B_Error *e) {
  struct B_IQuestion *native_question;
  struct B_QuestionVTable const *native_vtable;
  if (!b_py_question_and_vtable_(
      question, &native_question, &native_vtable, e)) {
    return false;
  }
  if (!native_vtable->serialize(
      native_question, byte_sink, e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_py_question_deserialize_(
    B_BORROW struct B_ByteSource *byte_source,
    B_OUT_TRANSFER struct B_IQuestion **question,
    B_OUT struct B_Error *e) {
  __builtin_trap();
  (void) byte_source;
  (void) question;
  (void) e;
}

// This template is copied and .uuid replaced for each
// B_PyQuestionVTable.
static struct B_QuestionVTable const
b_py_python_question_vtable_ = {
  // .uuid
  // .answer_vtable
  .query_answer = b_py_question_query_answer_,
  .deallocate = b_py_question_deallocate_,
  .replicate = b_py_question_replicate_,
  .serialize = b_py_question_serialize_,
  .deserialize = b_py_question_deserialize_,
};

static PyTypeObject
b_py_question_vtable_type_ = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_basicsize = sizeof(struct B_PyQuestionVTable),
  .tp_doc = "",
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_name = "_b.QuestionVTable",
};

static char const
b_py_question_vtable_key_[] = "b_vtable_internal_";

static B_WUR B_FUNC B_BORROW struct B_PyQuestionVTable *
b_py_question_class_vtable_(
    B_BORROW PyTypeObject *type) {
  PyObject *value = PyDict_GetItemString(
    type->tp_dict, b_py_question_vtable_key_);
  if (!value) {
    goto fail;
  }
  if (!b_py_question_vtable_check_(value)) {
    goto fail;
  }
  return (struct B_PyQuestionVTable *) value;
fail:
  PyErr_SetString(
    PyExc_TypeError, "Expected Question subclass");
  return NULL;
}

B_WUR B_FUNC B_BORROW struct B_QuestionVTable const *
b_py_question_class_get_native_vtable(
    B_BORROW PyTypeObject *type) {
  struct B_PyQuestionVTable *vtable_py
    = b_py_question_class_vtable_(type);
  if (!vtable_py) {
    return NULL;
  }
  return vtable_py->native_vtable;
}

B_WUR B_FUNC B_BORROW struct B_QuestionVTable const *
b_py_question_class_get_python_vtable(
    B_BORROW PyTypeObject *type) {
  struct B_PyQuestionVTable *vtable_py
    = b_py_question_class_vtable_(type);
  if (!vtable_py) {
    return NULL;
  }
  return &vtable_py->python_vtable;
}

B_WUR B_FUNC bool
b_py_question_class_set_native_vtable(
    B_BORROW PyTypeObject *type,
    B_BORROW struct B_QuestionVTable const *vtable) {
  struct B_PyQuestionVTable *vtable_py
    = (struct B_PyQuestionVTable *)
      b_py_question_vtable_type_.tp_alloc(
        &b_py_question_vtable_type_, 0);
  if (!vtable_py) {
    return false;
  }
  vtable_py->native_vtable = vtable;
  vtable_py->python_vtable = b_py_python_question_vtable_;
  vtable_py->python_vtable.uuid = vtable->uuid;
  vtable_py->python_vtable.answer_vtable
    = vtable->answer_vtable;  // TODO(strager)
  if (PyDict_SetItemString(
      type->tp_dict,
      b_py_question_vtable_key_,
      (PyObject *) vtable_py) == -1) {
    Py_DECREF(vtable_py);
    return false;
  }
  Py_DECREF(vtable_py);
  return true;
}

B_WUR B_FUNC bool
b_py_question_vtable_init(
    void) {
  if (PyType_Ready(&b_py_question_vtable_type_) < 0) {
    return NULL;
  }
  return true;
}
