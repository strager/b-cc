#include <B/AnswerContext.h>
#include <B/AnswerFuture.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/Main.h>
#include <B/QuestionAnswer.h>
#include <B/RunLoop.h>

#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    B_TRANSFER struct B_AnswerContext *ac,
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
  enum B_AnswerFutureState state;
  if (!b_answer_future_state(answer_future, &state, e)) {
    return false;
  }
  switch (state) {
  default:
    // BUG!
    abort();
  case B_FUTURE_RESOLVED:
    {
      struct B_Error error;
      if (!join_files_(ac, &error)) {
        if (!b_answer_context_fail(ac, error, e)) {
          return false;
        }
        return true;
      }
      return true;
    }
  case B_FUTURE_FAILED:
    if (!b_answer_context_fail(
        ac, (struct B_Error) {.posix_error = ENOENT}, e)) {
      return false;
    }
    return true;
  }
}

static bool
build_joined_(
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
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
      ac, questions, vtables, PATH_COUNT_, &future, e)) {
    return false;
  }
  for (size_t i = 0; i < PATH_COUNT_; ++i) {
    vtables[i]->deallocate(questions[i]);
    vtables[i] = b_file_question_vtable();
  }
  if (!b_answer_future_add_callback(
      future, join_callback_, &ac, sizeof(ac), e)) {
    return false;
  }
  b_answer_future_release(future);
  return true;
}

static B_FUNC bool
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
      ac, (struct B_Error) {.posix_error = EINVAL}, e)) {
    return false;
  }
  return true;
}

static B_FUNC bool
root_question_answered_(
    B_BORROW struct B_AnswerFuture *future,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *e) {
  struct B_RunLoop *rl
    = *(struct B_RunLoop *const *) opaque;
  if (!b_run_loop_stop(rl, e)) {
    return false;
  }
  return true;
}

bool
run_(
    int *exit_code,
    struct B_Error *e) {
  bool ok;

  struct B_Database *database = NULL;
  struct B_RunLoop *run_loop = NULL;
  struct B_Main *main = NULL;

  if (!b_database_open_sqlite3(
      "JoinFiles.cache",
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
      NULL,
      &database,
      e)) {
    database = NULL;
    goto fail;
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
      e)) {
    goto fail;
  }

  if (!b_run_loop_allocate_preferred(&run_loop, e)) {
    run_loop = NULL;
    goto fail;
  }

  if (!b_main_allocate(
      database,
      run_loop,
      dispatch_question_,
      NULL,
      &main,
      e)) {
    main = NULL;
    goto fail;
  }

  struct B_IQuestion *question;
  if (!b_file_question_allocate(
      "joined.txt", &question, e)) {
    goto fail;
  }
  struct B_AnswerFuture *answer_future;
  if (!b_main_answer(
      main,
      question,
      b_file_question_vtable(),
      &answer_future,
      e)) {
    b_file_question_vtable()->deallocate(question);
    goto fail;
  }
  b_file_question_vtable()->deallocate(question);

  if (!b_answer_future_add_callback(
      answer_future,
      root_question_answered_,
      &run_loop,
      sizeof(run_loop),
      e)) {
    goto fail;
  }

  if (!b_run_loop_run(run_loop, e)) {
    goto fail;
  }

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(answer_future, &state, e)) {
    b_answer_future_release(answer_future);
    goto fail;
  }
  b_answer_future_release(answer_future);
  answer_future = NULL;
  switch (state) {
  case B_FUTURE_PENDING:
    fprintf(stderr, "Answer future still PENDING\n");
    ok = true;
    *exit_code = 1;
    goto done;
  case B_FUTURE_FAILED:
    fprintf(stderr, "Answering question FAILED\n");
    ok = true;
    *exit_code = 2;
    goto done;
  case B_FUTURE_RESOLVED:
    break;
  }

  ok = true;
  *exit_code = 0;

done:
  if (main) {
    if (!b_main_deallocate(main, e)) {
      ok = false;
    }
  }
  if (run_loop) {
    b_run_loop_deallocate(run_loop);
  }
  if (database) {
    if (!b_database_close(database, e)) {
      ok = false;
    }
  }
  return ok;

fail:
  ok = false;
  goto done;
}

int
main(
    int argc,
    char **argv) {
  int exit_code;
  struct B_Error error;
  if (!run_(&exit_code, &error)) {
    fprintf(
      stderr, "Error: %s\n", strerror(error.posix_error));
    return 1;
  }
  return exit_code;
}
