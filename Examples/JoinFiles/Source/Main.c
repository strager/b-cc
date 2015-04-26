#include <B/AnswerContext.h>
#include <B/AnswerFuture.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/Main.h>
#include <B/QuestionAnswer.h>

#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
handle_error_(
    struct B_Error error) {
  fprintf(stderr, "%s\n", strerror(error.posix_error));
  exit(1);
}

static char const *const
joined_paths_[] = {
  "one.txt",
  "two.txt",
  "three.txt",
};

enum {PATH_COUNT_
  = sizeof(joined_paths_) / sizeof(*joined_paths_)};

static B_FUNC bool
join_files_(
    B_BORROW struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  FILE *joined = NULL;
  FILE *part = NULL;
  bool ok;

  joined = fopen("joined.txt", "w");
  if (!joined) {
    *e = (struct B_Error) {.posix_error = errno};
    goto fail;
  }
  for (size_t i = 0; i < PATH_COUNT_; ++i) {
    part = fopen(joined_paths_[i], "r");
    if (!part) {
      *e = (struct B_Error) {.posix_error = errno};
      goto fail;
    }
    char buffer[1024];
    while (!feof(part)) {
      size_t read = fread(buffer, 1, sizeof(buffer), part);
      {
        int error = ferror(part);
        if (error) {
          *e = (struct B_Error) {.posix_error = error};
          goto fail;
        }
      }
      size_t written = fwrite(buffer, 1, read, joined);
      {
        int error = ferror(joined);
        if (error) {
          *e = (struct B_Error) {.posix_error = error};
          goto fail;
        }
      }
      if (written != read) {
        // FIXME(strager)
        *e = (struct B_Error) {.posix_error = 0};
        goto fail;
      }
    }
    if (fflush(part) == EOF) {
      *e = (struct B_Error) {.posix_error = errno};
      goto fail;
    }
    (void) fclose(part);
    part = NULL;
  }
  if (fflush(joined) == EOF) {
    *e = (struct B_Error) {.posix_error = errno};
    goto fail;
  }
  if (!b_answer_context_succeed(ac, e)) {
    goto fail;
  }
  ok = true;

done:
  if (part) {
    (void) fclose(part);
  }
  if (joined) {
    (void) fclose(joined);
  }
  return ok;

fail:
  ok = false;
  goto done;
}

static B_FUNC bool
join_callback_(
    B_BORROW struct B_AnswerFuture *answer_future,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  struct B_AnswerContext *ac
    = *(struct B_AnswerContext *const *) callback_data;
  struct B_Error error;
  if (!join_files_(ac, &error)) {
    if (!b_answer_context_fail(ac, error, e)) {
      return false;
    }
    return true;
  }
  return true;
}

static bool
build_joined_(
    B_TRANSFER struct B_AnswerContext *ac,
    B_BORROW struct B_Error *e) {
  struct B_IQuestion *questions[PATH_COUNT_];
  struct B_QuestionVTable const *vtables[PATH_COUNT_];
  for (size_t i = 0; i < PATH_COUNT_; ++i) {
    if (!b_file_question_allocate(
        joined_paths_[i], &questions[i], e)) {
      // TODO(strager): Clean up properly.
      return false;
    }
    vtables[i] = b_file_question_vtable();
  }
  struct B_AnswerFuture *future;
  if (!b_answer_context_need(
      ac,
      questions,
      vtables,
      PATH_COUNT_,
      &future,
      e)) {
    return false;
  }
  for (size_t i = 0; i < PATH_COUNT_; ++i) {
    vtables[i]->deallocate(questions[i]);
    vtables[i] = b_file_question_vtable();
  }
  if (!b_answer_future_add_callback(
      future,
      join_callback_,
      &ac,
      sizeof(ac),
      e)) {
    return false;
  }
  b_answer_future_release(future);
  return true;
}

B_FUNC bool
dispatch_question_(
    B_BORROW void *opaque,
    B_BORROW struct B_Main *main,
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  struct B_IQuestion *question;
  struct B_QuestionVTable const *question_vtable;
  if (!b_answer_context_question(
      ac, &question, &question_vtable, e)) {
    return false;
  }
  char const *path;
  if (!b_file_question_path(question, &path, e)) {
    return false;
  }
  printf("dispatch_question(%s)\n", path);
  if (strcmp(path, "joined.txt") == 0) {
    if (!build_joined_(ac, e)) {
      return false;
    }
    return true;
  }
  for (size_t i = 0; i < PATH_COUNT_; ++i) {
    if (strcmp(path, joined_paths_[i]) == 0) {
      if (!b_answer_context_succeed(ac, e)) {
        return false;
      }
      return true;
    }
  }
  if (!b_answer_context_fail(
      ac,
      (struct B_Error) {.posix_error = EINVAL},
      e)) {
    return false;
  }
  return true;
}

int
main(
    int argc,
    char **argv) {
  struct B_Error e;

  struct B_Database *database;
  if (!b_database_open_sqlite3(
      "JoinFiles.cache",
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
      NULL,
      &database,
      &e)) {
    handle_error_(e);
  }
  struct B_QuestionVTable const *const vtables[] = {
    b_file_question_vtable(),
  };
  // FIXME(strager): Design problem: this does not interact
  // well with Main's AnswerCache.
  if (!b_database_check_all(
      database,
      vtables,
      sizeof(vtables) / sizeof(*vtables),
      &e)) {
    handle_error_(e);
  }

  struct B_Main *main;
  if (!b_main_allocate(
      database, dispatch_question_, NULL, &main, &e)) {
    handle_error_(e);
  }

  struct B_IQuestion *question;
  if (!b_file_question_allocate(
      "joined.txt", &question, &e)) {
    handle_error_(e);
  }
  struct B_AnswerFuture *answer_future;
  if (!b_main_answer(
      main,
      question,
      b_file_question_vtable(),
      &answer_future,
      &e)) {
    handle_error_(e);
  }
  b_file_question_vtable()->deallocate(question);

  bool keep_going = true;
  while (keep_going) {
    if (!b_main_loop(main, &keep_going, &e)) {
      handle_error_(e);
    }
  }

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(answer_future, &state, &e)) {
    handle_error_(e);
  }
  b_answer_future_release(answer_future);
  answer_future = NULL;
  switch (state) {
  case B_FUTURE_PENDING:
    fprintf(stderr, "Answer future still PENDING\n");
    exit(1);
  case B_FUTURE_FAILED:
    fprintf(stderr, "Answering question FAILED\n");
    exit(1);
  case B_FUTURE_RESOLVED:
    break;
  }

  if (!b_main_deallocate(main, &e)) {
    handle_error_(e);
  }
  if (!b_database_close(database, &e)) {
    handle_error_(e);
  }

  return 0;
}
