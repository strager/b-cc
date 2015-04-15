#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/Private/Assertions.h>
#include <B/Private/Memory.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static B_WUR B_FUNC bool
b_file_question_deallocate_(
    B_TRANSFER struct B_IQuestion *question,
    struct B_Error *e) {
  B_PRECONDITION(question);
  B_OUT_PARAMETER(e);

  if (!b_deallocate(question, e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_file_question_query_answer_(
    B_BORROW struct B_IQuestion *question,
    B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **out,
    struct B_Error *e) {
  B_PRECONDITION(question);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  FILE *f = NULL;
  bool ok;

  char const *path = (char const *) question;
  // TODO(strager): Something better than summation.
  f = fopen(path, "rb");
  if (!f) {
    int error = errno;
    if (error == ENOENT) {
      *out = NULL;
      return true;
    }
    *e = (struct B_Error) {.posix_error = error};
    goto fail;
  }
  uint64_t counter = 0;
  char buffer[1024];
  while (!feof(f)) {
    size_t read_bytes = fread(buffer, 1, sizeof(buffer), f);
    B_ASSERT(read_bytes <= sizeof(buffer));
    int error = ferror(f);
    if (error) {
      *e = (struct B_Error) {.posix_error = error};
      goto fail;
    }
    for (size_t i = 0; i < read_bytes; ++i) {
      counter += (uint8_t) buffer[i];
    }
  }
  uint64_t *answer;
  if (!b_allocate(sizeof(*answer), (void **) &answer, e)) {
    goto fail;
  }
  *answer = counter;
  *out = (struct B_IAnswer *) answer;
  ok = true;

done:
  if (f) {
    (void) fclose(f);
  }
  return ok;

fail:
  ok = false;
  goto done;
}

static B_WUR B_FUNC bool
b_file_question_serialize_(
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_ByteSink *sink,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(question);
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  char const *path = (char const *) question;
  size_t length = strlen(path);
  if (!b_serialize_data_and_size_8_be(
      sink, (uint8_t const *) path, length, e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_file_question_deserialize_(
    B_BORROW struct B_ByteSource *source,
    B_OUT_TRANSFER struct B_IQuestion **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(source);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint64_t length_64;
  if (!b_deserialize_8_be(source, &length_64, e)) {
    return false;
  }
  if (length_64 > SIZE_MAX) {
    *e = (struct B_Error) {.posix_error = ENOMEM};
    return false;
  }
  size_t length = (size_t) length_64;
  char *path;
  if (!b_allocate(length + 1, (void **) &path, e)) {
    return false;
  }
  if (!b_deserialize_bytes(
      source, length, (uint8_t *) path, e)) {
    (void) b_deallocate(path, &(struct B_Error) {});
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    if (path[i] == '\0') {
      // Embedded \0; data is likely corrupt.
      *e = (struct B_Error) {.posix_error = EINVAL};
      return false;
    }
  }
  path[length] = '\0';
  *out = (struct B_IQuestion *) path;
  return true;
}

static B_WUR B_FUNC bool
b_file_answer_deallocate_(
    B_TRANSFER struct B_IAnswer *answer,
    struct B_Error *e) {
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(e);

  if (!b_deallocate(answer, e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_file_answer_replicate_(
    B_BORROW struct B_IAnswer const *answer,
    B_OUT_TRANSFER struct B_IAnswer **out,
    struct B_Error *e) {
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint64_t *new_hash;
  if (!b_allocate(
      sizeof(*new_hash), (void **) &new_hash, e)) {
    return false;
  }
  *new_hash = *(uint64_t const *) answer;
  *out = (struct B_IAnswer *) new_hash;
  return true;
}

static B_WUR B_FUNC bool
b_file_answer_serialize_(
    B_BORROW struct B_IAnswer const *answer,
    B_BORROW struct B_ByteSink *sink,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(answer);
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  uint64_t hash = *(uint64_t const *) answer;
  if (!b_serialize_8_be(sink, hash, e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
b_file_answer_deserialize_(
    B_BORROW struct B_ByteSource *source,
    B_OUT_TRANSFER struct B_IAnswer **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(source);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  uint64_t *hash;
  if (!b_allocate(sizeof(*hash), (void **) &hash, e)) {
    return false;
  }
  if (!b_deserialize_8_be(source, hash, e)) {
    (void) b_deallocate(hash, &(struct B_Error) {});
    return false;
  }
  *out = (struct B_IAnswer *) hash;
  return true;
}

static struct B_AnswerVTable const
b_file_answer_vtable_ = {
  .deallocate = b_file_answer_deallocate_,
  .replicate = b_file_answer_replicate_,
  .serialize = b_file_answer_serialize_,
  .deserialize = b_file_answer_deserialize_,
};

static struct B_QuestionVTable const
b_file_question_vtable_ = {
  .answer_vtable = &b_file_answer_vtable_,
  .deallocate = b_file_question_deallocate_,
  .query_answer = b_file_question_query_answer_,
  .serialize = b_file_question_serialize_,
  .deserialize = b_file_question_deserialize_,
};

B_WUR B_EXPORT_FUNC struct B_QuestionVTable const *
b_file_question_vtable(
    void) {
  return &b_file_question_vtable_;
}

B_WUR B_EXPORT_FUNC bool
b_file_question_allocate(
    B_BORROW char const *file_path,
    B_OUT_TRANSFER struct B_IQuestion **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(file_path);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_IQuestion *q;
  if (!b_strdup(file_path, (char **) &q, e)) {
    return false;
  }
  *out = q;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_file_question_path(
    B_BORROW struct B_IQuestion const *question,
    B_OUT_BORROW char const **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(question);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  *out = (char const *) question;
  return true;
}
