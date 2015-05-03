#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/Memory.h>
#include <B/Private/Assertions.h>
#include <B/Private/Memory.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

#include <errno.h>
#include <sph_sha2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct B_FileAnswer_ {
  uint8_t sha256_hash[SPH_SIZE_sha256 / 8];
};

static B_WUR B_FUNC void
b_file_question_deallocate_(
    B_TRANSFER struct B_IQuestion *question) {
  B_PRECONDITION(question);

  b_deallocate(question);
}

static B_WUR B_FUNC bool
b_file_question_replicate_(
    B_BORROW struct B_IQuestion const *question,
    B_OUT_TRANSFER struct B_IQuestion **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(question);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_IQuestion *new_question;
  if (!b_file_question_allocate(
      (char const *) question, &new_question, e)) {
    return false;
  }
  *out = new_question;
  return true;
}

static B_WUR B_FUNC bool
b_file_question_query_answer_(
    B_BORROW struct B_IQuestion const *question,
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
  sph_sha256_context sha256_context;
  sph_sha256_init(&sha256_context);
  {
    char buffer[1024];
    while (!feof(f)) {
      size_t read_bytes = fread(
        buffer, 1, sizeof(buffer), f);
      B_ASSERT(read_bytes <= sizeof(buffer));
      int error = ferror(f);
      if (error) {
        *e = (struct B_Error) {.posix_error = error};
        goto fail;
      }
      sph_sha256(&sha256_context, buffer, read_bytes);
    }
  }
  struct B_FileAnswer_ *answer;
  if (!b_allocate(sizeof(*answer), (void **) &answer, e)) {
    goto fail;
  }
  sph_sha256_close(&sha256_context, answer->sha256_hash);
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
    b_deallocate(path);
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

static B_WUR B_FUNC void
b_file_answer_deallocate_(
    B_TRANSFER struct B_IAnswer *answer) {
  B_PRECONDITION(answer);

  b_deallocate(answer);
}

static B_WUR B_FUNC bool
b_file_answer_replicate_(
    B_BORROW struct B_IAnswer const *answer,
    B_OUT_TRANSFER struct B_IAnswer **out,
    struct B_Error *e) {
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_FileAnswer_ *new_answer;
  if (!b_allocate(
      sizeof(*new_answer), (void **) &new_answer, e)) {
    return false;
  }
  *new_answer = *(struct B_FileAnswer_ const *) answer;
  *out = (struct B_IAnswer *) new_answer;
  return true;
}

static B_WUR B_FUNC bool
b_file_answer_serialize_(
    B_BORROW struct B_IAnswer const *raw_answer,
    B_BORROW struct B_ByteSink *sink,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(raw_answer);
  B_PRECONDITION(sink);
  B_OUT_PARAMETER(e);

  struct B_FileAnswer_ const *answer
    = (struct B_FileAnswer_ const *) raw_answer;
  if (!b_serialize_bytes(
      sink,
      answer->sha256_hash,
      sizeof(answer->sha256_hash),
      e)) {
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

  struct B_FileAnswer_ *answer;
  if (!b_allocate(sizeof(*answer), (void **) &answer, e)) {
    return false;
  }
  if (!b_deserialize_bytes(
      source,
      sizeof(answer->sha256_hash),
      answer->sha256_hash,
      e)) {
    b_deallocate(answer);
    return false;
  }
  *out = (struct B_IAnswer *) answer;
  return true;
}

static B_WUR B_FUNC bool
b_file_answer_equal_(
    B_BORROW struct B_IAnswer const *raw_a,
    B_BORROW struct B_IAnswer const *raw_b) {
  B_PRECONDITION(raw_a);
  B_PRECONDITION(raw_b);

  struct B_FileAnswer_ const *a
    = (struct B_FileAnswer_ const *) raw_a;
  struct B_FileAnswer_ const *b
    = (struct B_FileAnswer_ const *) raw_b;
  if (memcmp(
      a->sha256_hash,
      b->sha256_hash,
      sizeof(a->sha256_hash)) != 0) {
    return false;
  }
  return true;
}

static struct B_AnswerVTable const
b_file_answer_vtable_ = {
  .deallocate = b_file_answer_deallocate_,
  .replicate = b_file_answer_replicate_,
  .serialize = b_file_answer_serialize_,
  .deserialize = b_file_answer_deserialize_,
  .equal = b_file_answer_equal_,
};

static struct B_QuestionVTable const
b_file_question_vtable_ = {
  .answer_vtable = &b_file_answer_vtable_,
  .deallocate = b_file_question_deallocate_,
  .replicate = b_file_question_replicate_,
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
