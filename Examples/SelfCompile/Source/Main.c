#include <B/AnswerContext.h>
#include <B/AnswerFuture.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/Main.h>
#include <B/Process.h>
#include <B/QuestionAnswer.h>
#include <B/RunLoop.h>

#include <assert.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const
out_path_[] = "SelfCompile";

static char const *
sources_[] = {
  "Examples/SelfCompile/Source/Main.c",
  "Source/AnswerContext.c",
  "Source/AnswerFuture.c",
  "Source/Assertions.c",
  "Source/Database.c",
  "Source/FileQuestion.c",
  "Source/Main.c",
  "Source/Memory.c",
  "Source/Mutex.c",
  "Source/QuestionAnswer.c",
  "Source/RunLoop.c",
  "Source/RunLoopKqueue.c",
  "Source/RunLoopSigchld.c",
  "Source/RunLoopUtil.c",
  "Source/SQLite3.c",
  "Source/Serialize.c",
  "Source/UUID.c",
  "Vendor/sphlib-3.0/c/sha2.c",
  "Vendor/sqlite-3.8.4.1/sqlite3.c",
};
enum {
  SOURCE_COUNT_ = sizeof(sources_) / sizeof(*sources_),
};

static void
print_command_(
    char const *const *args) {
  for (char const *const *arg = args; *arg; ++arg) {
    fprintf(stderr, "%s%c", *arg, arg[1] ? ' ' : '\n');
  }
}

static B_FUNC bool
exec_callback_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW struct B_ProcessExitStatus const *status,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *e) {
  (void) rl;
  struct B_AnswerContext *ac
    = *(struct B_AnswerContext *const *) opaque;
  if (status->type == B_PROCESS_EXIT_STATUS_CODE
      && status->u.code.exit_code == 0) {
    if (!b_answer_context_succeed(ac, e)) {
      return false;
    }
    return true;
  } else {
    if (!b_answer_context_fail(
        ac, (struct B_Error) {.posix_error = ENOENT}, e)) {
      return false;
    }
    return true;
  }
}

static B_FUNC bool
exec_cancel_callback_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *e) {
  (void) rl;
  struct B_AnswerContext *ac
    = *(struct B_AnswerContext *const *) opaque;
  if (!b_answer_context_fail(
      ac, (struct B_Error) {.posix_error = ENOENT}, e)) {
    return false;
  }
  return true;
}

static bool
exec_(
    B_TRANSFER struct B_AnswerContext *ac,
    char const *const *command_args,
    B_OUT struct B_Error *e) {
  struct B_RunLoop *rl;
  if (!b_answer_context_run_loop(ac, &rl, e)) {
    return false;
  }
  print_command_(command_args);
  if (!b_run_loop_exec_basic(
      rl,
      command_args,
      exec_callback_,
      exec_cancel_callback_,
      &ac,
      sizeof(ac),
      e)) {
    return false;
  }
  return true;
}

static B_FUNC bool
link_exec_(
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  static char const *command_prefix[] = {
    "cc",
    "-o", out_path_,
  };
  static char const *command_suffix[] = {
    "-ldl",  // For Linux.
    "-lpthread",  // For Linux.
  };
  enum {
    COMMAND_PREFIX_COUNT_
      = sizeof(command_prefix) / sizeof(*command_prefix),
    COMMAND_SUFFIX_COUNT_
      = sizeof(command_suffix) / sizeof(*command_suffix),
  };
  char const *command[
    COMMAND_PREFIX_COUNT_ + SOURCE_COUNT_ +
    COMMAND_SUFFIX_COUNT_ + 1];
  size_t arg = 0;
  for (size_t i = 0; i < COMMAND_PREFIX_COUNT_; ++i) {
    command[arg++] = command_prefix[i];
  }
  for (size_t i = 0; i < SOURCE_COUNT_; ++i) {
    char *obj = strdup(sources_[i]);
    assert(obj);
    obj[strlen(obj) - 1] = 'o';  // Replace extension.
    command[arg++] = obj;
  }
  for (size_t i = 0; i < COMMAND_SUFFIX_COUNT_; ++i) {
    command[arg++] = command_suffix[i];
  }
  command[arg++] = NULL;
  if (!exec_(ac, command, e)) {
    return false;
  }
  for (size_t i = 0; i < SOURCE_COUNT_; ++i) {
    free((char *) command[COMMAND_PREFIX_COUNT_ + i]);
  }
  return true;
}

static B_FUNC bool
link_callback_(
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
      if (!link_exec_(ac, &error)) {
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
link_(
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  struct B_IQuestion *questions[SOURCE_COUNT_] = {};
  struct B_QuestionVTable const *vtables[SOURCE_COUNT_];
  for (size_t i = 0; i < SOURCE_COUNT_; ++i) {
    char *obj_path = strdup(sources_[i]);
    assert(obj_path);
    obj_path[strlen(obj_path) - 1]
      = 'o';  // Replace extension.
    if (!b_file_question_allocate(
        obj_path, &questions[i], e)) {
      // TODO(strager): Clean up properly.
      return false;
    }
    free(obj_path);
    vtables[i] = b_file_question_vtable();
  }
  struct B_AnswerFuture *future;
  if (!b_answer_context_need(
      ac, questions, vtables, SOURCE_COUNT_, &future, e)) {
    return false;
  }
  for (size_t i = 0; i < SOURCE_COUNT_; ++i) {
    vtables[i]->deallocate(questions[i]);
    vtables[i] = b_file_question_vtable();
  }
  if (!b_answer_future_add_callback(
      future, link_callback_, &ac, sizeof(ac), e)) {
    return false;
  }
  b_answer_future_release(future);
  return true;
}

static B_FUNC bool
compile_exec_(
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  struct B_IQuestion *question;
  struct B_QuestionVTable const *question_vtable;
  if (!b_answer_context_question(
      ac, &question, &question_vtable, e)) {
    return false;
  }
  char const *obj_path;
  if (!b_file_question_path(question, &obj_path, e)) {
    return false;
  }
  char *c_path = strdup(obj_path);
  assert(c_path);
  c_path[strlen(c_path) - 1] = 'c';  // Replace extension.
  char const *command[] = {
    "cc",
    "-I", "Headers",
    "-I", "PrivateHeaders",
    "-I", "Vendor/sphlib-3.0/c",
    "-I", "Vendor/sqlite-3.8.4.1",
    "-std=c99",
    // See various CMakeLists.txt.
    "-D_POSIX_SOURCE",
    "-D_POSIX_C_SOURCE=200809L",
    "-D_DARWIN_C_SOURCE",
    "-o", obj_path,
    "-c",
    c_path,
    NULL,
  };
  if (!exec_(ac, command, e)) {
    free(c_path);
    return false;
  }
  free(c_path);
  return true;
}

static B_FUNC bool
compile_callback_(
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
      if (!compile_exec_(ac, &error)) {
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
compile_(
    B_BORROW char const *obj_path,
    B_TRANSFER struct B_AnswerContext *ac,
    B_OUT struct B_Error *e) {
  char *c_path = strdup(obj_path);
  assert(c_path);
  c_path[strlen(c_path) - 1] = 'c';  // Replace extension.
  struct B_IQuestion *c_question;
  if (!b_file_question_allocate(c_path, &c_question, e)) {
    free(c_path);
    return false;
  }
  free(c_path);
  struct B_AnswerFuture *future;
  if (!b_answer_context_need_one(
      ac,
      c_question,
      b_file_question_vtable(),
      &future,
      e)) {
    b_file_question_vtable()->deallocate(c_question);
    return false;
  }
  b_file_question_vtable()->deallocate(c_question);
  if (!b_answer_future_add_callback(
      future, compile_callback_, &ac, sizeof(ac), e)) {
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
  if (strcmp(path, out_path_) == 0) {
    if (!link_(ac, e)) {
      return false;
    }
    return true;
  }
  char const *extension = strrchr(path, '.');
  if (extension && strcmp(extension, ".o") == 0) {
    if (!compile_(path, ac, e)) {
      return false;
    }
    return true;
  }
  if (extension && strcmp(extension, ".c") == 0) {
    if (!b_answer_context_succeed(ac, e)) {
      return false;
    }
    return true;
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
      "SelfCompile.cache",
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
      out_path_, &question, e)) {
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
