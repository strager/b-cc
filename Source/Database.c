// NOTE[schema]: There are two tables: dependencies and
// answers.

#include <B/Database.h>
#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/Private/Mutex.h>
#include <B/Private/SQLite3.h>
#include <B/QuestionAnswer.h>
#include <B/UUID.h>
//#include <B/Serialize.h>

#include <errno.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct Buffer_ {
  uint8_t *data;
  size_t size;
};

// NOTE[insert dependency query]: These are host parameter
// names for b_database_record_dependency's INSERT query.
enum {
  B_INSERT_DEPENDENCY_FROM_QUESTION_UUID = 1,
  B_INSERT_DEPENDENCY_FROM_QUESTION_DATA = 2,
  B_INSERT_DEPENDENCY_TO_QUESTION_UUID = 3,
  B_INSERT_DEPENDENCY_TO_QUESTION_DATA = 4,
};

// NOTE[insert answer query]: These are host parameter names
// for b_database_record_answer's INSERT query.
enum {
  B_INSERT_ANSWER_QUESTION_UUID = 1,
  B_INSERT_ANSWER_QUESTION_DATA = 2,
  B_INSERT_ANSWER_ANSWER_DATA = 3,
};

// NOTE[select answer query]: These are host parameter names
// for b_database_look_up_answer's SELECT query.
enum {
  B_SELECT_ANSWER_QUESTION_UUID = 1,
  B_SELECT_ANSWER_QUESTION_DATA = 2,
};

// NOTE[select answer query]: These are column indices for
// results of b_database_look_up_answer's SELECT query.
enum {
  B_SELECT_ANSWER_ANSWER_DATA = 0,
};

// NOTE[recheck all answers query]: There are no inputs or
// outputs for the b_database_recheck_all's query.  Instead,
// a user-defined function is queried.
//
// TODO(strager): sqlite3_create_function to check Answer-s,
// ON CASCADE DELETE to delete dependencies.
enum {
  B_SELECT_ALL_ANSWERS_QUESTION_UUID = 1,
  B_SELECT_ALL_ANSWERS_QUESTION_DATA = 2,
  B_SELECT_ALL_ANSWERS_ANSWER_DATA = 3,
};

struct B_Database {
  struct B_Mutex lock;

  sqlite3 *handle;
  sqlite3_stmt *insert_dependency_stmt;
  sqlite3_stmt *insert_answer_stmt;
  sqlite3_stmt *select_answer_stmt;
  sqlite3_stmt *recheck_all_answers_stmt;

  // Fields for UDFs (User Defined Functions).  Temporary.
  struct {
    B_BORROW struct B_QuestionVTable const *const *vtables;
    size_t vtable_count;
  } udf;
};

static B_WUR B_FUNC bool
prepare_database_locked_(
    B_BORROW struct B_Database *,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
record_answer_locked_(
    B_BORROW struct B_Database *,
    B_BORROW struct B_IQuestion const *,
    B_BORROW struct B_QuestionVTable const *,
    B_BORROW struct B_IAnswer const *,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
insert_answer_locked_(
    B_BORROW struct B_Database *,
    B_TRANSFER struct Buffer_ question_data,
    B_BORROW struct B_UUID const question_uuid,
    B_TRANSFER struct Buffer_ answer_data,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
record_dependency_locked_(
    B_BORROW struct B_Database *,
    B_BORROW struct B_IQuestion const *from,
    B_BORROW struct B_QuestionVTable const *from_vtable,
    B_BORROW struct B_IQuestion const *to,
    B_BORROW struct B_QuestionVTable const *to_vtable,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
insert_dependency_locked_(
    B_BORROW struct B_Database *,
    B_TRANSFER struct Buffer_ from_data,
    struct B_UUID const from_uuid,
    B_TRANSFER struct Buffer_ to_data,
    struct B_UUID const to_uuid,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
look_up_answer_locked_(
    B_BORROW struct B_Database *,
    B_BORROW struct B_IQuestion const *,
    B_BORROW struct B_QuestionVTable const *,
    B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
b_check_all_locked_(
    B_BORROW struct B_Database *,
    B_BORROW struct B_QuestionVTable const *const *,
    size_t question_vtable_count,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
bind_uuid_(
    B_BORROW sqlite3_stmt *,
    int host_parameter_name,
    struct B_UUID,
    B_OUT struct B_Error *);

static B_WUR B_FUNC bool
bind_buffer_(
    B_BORROW sqlite3_stmt *,
    int host_parameter_name,
    B_TRANSFER struct Buffer_,
    B_OUT struct B_Error *);

static void
deallocate_buffer_data_(
    B_TRANSFER void *data);

static void
question_answer_matches_locked_(
    B_BORROW sqlite3_context *,
    int arg_count,
    B_BORROW sqlite3_value **args);

static B_WUR B_FUNC bool
value_uuid_(
    B_BORROW sqlite3_value *,
    B_OUT struct B_UUID *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_database_open_sqlite3(
    B_BORROW char const *sqlite_path,
    int sqlite_flags,
    B_BORROW_OPTIONAL char const *sqlite_vfs,
    B_OUT_TRANSFER struct B_Database **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(sqlite_path);
  B_PRECONDITION(out);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  // Ensure sqlite3 features used are available.
#define B_REQUIRED_SQLITE_VERSION_NUMBER_ 3008003
#if SQLITE_VERSION_NUMBER \
  < B_REQUIRED_SQLITE_VERSION_NUMBER_
# error "sqlite3 version >=3.8.3 required"
#endif
  if (sqlite3_libversion_number()
      < B_REQUIRED_SQLITE_VERSION_NUMBER_) {
    *e = (struct B_Error) {.posix_error = ENOTSUP};
    return false;
  }
  if (sqlite3_compileoption_used("SQLITE_OMIT_CTE")) {
    *e = (struct B_Error) {.posix_error = ENOTSUP};
    return false;
  }

  struct B_Database *database;
  if (!b_allocate(
      sizeof(*database), (void **) &database, e)) {
    return false;
  }
  *database = (struct B_Database) {
    // .lock
    .handle = NULL,
    .insert_dependency_stmt = NULL,
    .insert_answer_stmt = NULL,
    .select_answer_stmt = NULL,
    .recheck_all_answers_stmt = NULL,
    .udf = {
      .vtables = NULL,
      .vtable_count = 0,
    },
  };
  if (!b_mutex_initialize(&database->lock, e)) {
    B_NYI();
    goto fail;
  }
  int rc = sqlite3_open_v2(
    sqlite_path,
    &database->handle,
    sqlite_flags,
    sqlite_vfs);
  if (rc != SQLITE_OK) {
    if (database->handle) {
      // "A database connection handle is usually
      // returned in *ppDb, even if an error occurs.
      // The only exception is that if SQLite is
      // unable to allocate memory to hold the sqlite3
      // object, a NULL will be written into *ppDb
      // instead of a pointer to the sqlite3 object."
      (void) sqlite3_close(database->handle);
      database->handle = NULL;
    }
    *e = b_sqlite3_error(rc);
    goto fail;
  }
  if (!prepare_database_locked_(database, e)) {
    // NOTE[unprepare database]: database is allowed to
    // be half-initialized.  b_database_close will
    // properly uninitialize only initialized prepared
    // statements.
    goto fail;
  }
  *out = database;
  return true;

fail:
  (void) b_database_close(database, &(struct B_Error) {});
  return false;
}

B_WUR B_EXPORT_FUNC bool
b_database_close(
    B_TRANSFER struct B_Database *database,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_OUT_PARAMETER(e);

  B_ASSERT(!database->udf.vtables);

  // TODO(strager): Report errors.
  if (database->insert_dependency_stmt) {
    (void) sqlite3_finalize(
      database->insert_dependency_stmt);
  }
  if (database->insert_answer_stmt) {
    (void) sqlite3_finalize(
      database->insert_answer_stmt);
  }
  if (database->select_answer_stmt) {
    (void) sqlite3_finalize(
      database->select_answer_stmt);
  }
  if (database->recheck_all_answers_stmt) {
    (void) sqlite3_finalize(
      database->recheck_all_answers_stmt);
  }
  if (database->handle) {
    (void) sqlite3_close(database->handle);
  }
  b_deallocate(database);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_database_record_dependency(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_IQuestion const *from,
    B_BORROW struct B_QuestionVTable const *from_vtable,
    B_BORROW struct B_IQuestion const *to,
    B_BORROW struct B_QuestionVTable const *to_vtable,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(from);
  B_PRECONDITION(from_vtable);
  B_PRECONDITION(to);
  B_PRECONDITION(to_vtable);
  B_OUT_PARAMETER(e);

  bool ok = true;
  b_mutex_lock(&database->lock);
  {
    ok = record_dependency_locked_(
      database, from, from_vtable, to, to_vtable, e);
  }
  b_mutex_unlock(&database->lock);
  return ok;
}

B_WUR B_EXPORT_FUNC bool
b_database_record_answer(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_BORROW struct B_IAnswer const *answer,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(e);

  bool ok = true;
  b_mutex_lock(&database->lock);
  {
    ok = record_answer_locked_(
      database, question, question_vtable, answer, e);
  }
  b_mutex_unlock(&database->lock);
  return ok;
}

B_WUR B_EXPORT_FUNC bool
b_database_look_up_answer(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_OUT_PARAMETER(out);

  bool ok = true;
  b_mutex_lock(&database->lock);
  {
    ok = look_up_answer_locked_(
      database, question, question_vtable, out, e);
  }
  b_mutex_unlock(&database->lock);
  return ok;
}

B_WUR B_EXPORT_FUNC bool
b_database_check_all(
    struct B_Database *database,
    B_BORROW struct B_QuestionVTable const *const *vtables,
    size_t vtable_count,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(vtables);
  B_OUT_PARAMETER(e);

  bool ok = true;
  b_mutex_lock(&database->lock);
  {
    ok = b_check_all_locked_(
      database, vtables, vtable_count, e);
  }
  b_mutex_unlock(&database->lock);
  return ok;
}

static B_WUR B_FUNC bool
prepare_database_locked_(
    B_BORROW struct B_Database *database,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(database->handle);
  B_PRECONDITION(!database->insert_dependency_stmt);
  B_PRECONDITION(!database->insert_answer_stmt);
  B_PRECONDITION(!database->select_answer_stmt);
  B_PRECONDITION(!database->recheck_all_answers_stmt);
  B_OUT_PARAMETER(e);

  sqlite3 *handle = database->handle;
  int rc;

  // See NOTE[b_question_answer_matches].
  rc = sqlite3_create_function_v2(
    handle,
    "b_question_answer_matches",
    3,
    SQLITE_DETERMINISTIC | SQLITE_UTF8,
    database,
    question_answer_matches_locked_,
    NULL,
    NULL,
    NULL);
  if (rc != SQLITE_OK) {
    *e = b_sqlite3_error(rc);
    goto fail;
  }

  // See NOTE[schema].
  static char const create_table_query[] = ""
    "CREATE TABLE IF NOT EXISTS dependencies(\n"
    "  from_question_uuid BLOB NOT NULL,\n"
    "  from_question_data BLOB NOT NULL,\n"
    "  to_question_uuid BLOB NOT NULL,\n"
    "  to_question_data BLOB NOT NULL);\n"
    "CREATE TABLE IF NOT EXISTS answers(\n"
    "  question_uuid BLOB NOT NULL,\n"
    "  question_data BLOB NOT NULL,\n"
    "  answer_data BLOB NOT NULL);\n";
  rc = sqlite3_exec(
    handle, create_table_query, NULL, NULL, NULL);
  if (rc != SQLITE_OK) {
    *e = b_sqlite3_error(rc);
    goto fail;
  }

  // See NOTE[insert dependency query].
  static char const insert_dependency_query[] = ""
    "INSERT INTO dependencies(\n"
    "  from_question_uuid,\n"
    "  from_question_data,\n"
    "  to_question_uuid,\n"
    "  to_question_data)\n"
    "VALUES (?1, ?2, ?3, ?4);";
  if (!b_sqlite3_prepare(
      handle,
      insert_dependency_query,
      sizeof(insert_dependency_query),
      &database->insert_dependency_stmt,
      e)) {
    goto fail;
  }

  // See NOTE[insert answer query].
  static char const insert_answer_query[] = ""
    "INSERT INTO answers(\n"
    "  question_uuid,\n"
    "  question_data,\n"
    "  answer_data)\n"
    "VALUES (?1, ?2, ?3);";
  if (!b_sqlite3_prepare(
      handle,
      insert_answer_query,
      sizeof(insert_answer_query),
      &database->insert_answer_stmt,
      e)) {
    goto fail;
  }

  // See NOTE[select answer query].
  static char const select_answer_query[] = ""
    "SELECT answer_data\n"
    "  FROM answers\n"
    "  WHERE question_uuid = ?1\n"
    "    AND question_data = ?2;";
  if (!b_sqlite3_prepare(
      handle,
      select_answer_query,
      sizeof(select_answer_query),
      &database->select_answer_stmt,
      e)) {
    goto fail;
  }

  // See NOTE[recheck all answers query].
  static char const recheck_all_answers_query[] = ""
    "WITH RECURSIVE invalid_answers(\n"
    "    question_uuid,\n"
    "    question_data) AS (\n"
    "  -- Seed recursion with out-of-date answers."
    "  -- See NOTE[b_question_answer_matches].\n"
    "  SELECT question_uuid, question_data\n"
    "    FROM answers\n"
    "    WHERE b_question_answer_matches(\n"
    "          question_uuid,\n"
    "          question_data,\n"
    "          answer_data) == 0\n"
    "\n"
    "  UNION ALL\n"
    "\n"
    "  -- Walk up the dependency graph.\n"
    "  SELECT dep.from_question_uuid,\n"
    "       dep.from_question_data\n"
    "    FROM invalid_answers AS invalid\n"
    "    INNER JOIN dependencies AS dep\n"
    "    ON dep.to_question_uuid = invalid.question_uuid\n"
    "       AND dep.to_question_data = invalid.question_data\n"
    ")\n"
    "-- Delete encountered rows.\n"
    "DELETE FROM answers WHERE _rowid_ IN (\n"
    "  SELECT answers._rowid_ FROM answers\n"
    "    INNER JOIN invalid_answers AS invalid\n"
    "    ON answers.question_uuid = invalid.question_uuid\n"
    "       AND answers.question_data = invalid.question_data);";
  if (!b_sqlite3_prepare(
      handle,
      recheck_all_answers_query,
      sizeof(recheck_all_answers_query),
      &database->recheck_all_answers_stmt,
      e)) {
    goto fail;
  }

  return true;

fail:
  // See NOTE[unprepare database].
  return false;
}

static B_WUR B_FUNC bool
record_answer_locked_(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_BORROW struct B_IAnswer const *answer,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(e);

  struct Buffer_ question_buffer = {
    .data = NULL,
    .size = 0,
  };
  struct Buffer_ answer_buffer = {
    .data = NULL,
    .size = 0,
  };
  if (!b_question_serialize_to_memory(
      question,
      question_vtable,
      &question_buffer.data,
      &question_buffer.size,
      e)) {
    goto fail;
  }
  if (!b_answer_serialize_to_memory(
      answer,
      question_vtable->answer_vtable,
      &answer_buffer.data,
      &answer_buffer.size,
      e)) {
    goto fail;
  }
  return insert_answer_locked_(
    database,
    question_buffer,
    question_vtable->uuid,
    answer_buffer,
    e);

fail:
  if (question_buffer.data) {
    b_deallocate(question_buffer.data);
  }
  if (answer_buffer.data) {
    b_deallocate(answer_buffer.data);
  }
  return false;
}

static B_WUR B_FUNC bool
insert_answer_locked_(
    B_BORROW struct B_Database *database,
    B_TRANSFER struct Buffer_ question_data,
    B_BORROW struct B_UUID const question_uuid,
    B_TRANSFER struct Buffer_ answer_data,
    B_OUT struct B_Error *e) {
  sqlite3_stmt *stmt = database->insert_answer_stmt;

  bool ok;

  bool need_free_question_data = true;
  bool need_free_answer_data = true;

  ok = bind_buffer_(
    stmt, B_INSERT_ANSWER_QUESTION_DATA, question_data, e);
  // FIXME(strager): Is this correct?
  need_free_question_data = false;
  if (!ok) goto done_no_reset;

  ok = bind_uuid_(
    stmt, B_INSERT_ANSWER_QUESTION_UUID, question_uuid, e);
  if (!ok) goto done_no_reset;

  ok = bind_buffer_(
    stmt, B_INSERT_ANSWER_ANSWER_DATA, answer_data, e);
  // FIXME(strager): Is this correct?
  need_free_answer_data = false;
  if (!ok) goto done_no_reset;

  ok = b_sqlite3_step_expecting_end(stmt, e);
  // TODO(strager): Error reporting.
  (void) sqlite3_reset(stmt);

done_no_reset:
  (void) sqlite3_clear_bindings(stmt);
  if (need_free_question_data) {
    b_deallocate(question_data.data);
  }
  if (need_free_answer_data) {
    b_deallocate(answer_data.data);
  }
  return ok;
}

static B_WUR B_FUNC bool
record_dependency_locked_(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_IQuestion const *from,
    B_BORROW struct B_QuestionVTable const *from_vtable,
    B_BORROW struct B_IQuestion const *to,
    B_BORROW struct B_QuestionVTable const *to_vtable,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(from);
  B_PRECONDITION(from_vtable);
  B_PRECONDITION(to);
  B_PRECONDITION(to_vtable);
  B_OUT_PARAMETER(e);

  struct Buffer_ from_buffer = {
    .data = NULL,
    .size = 0,
  };
  struct Buffer_ to_buffer = {
    .data = NULL,
    .size = 0,
  };
  if (!b_question_serialize_to_memory(
      from,
      from_vtable,
      &from_buffer.data,
      &from_buffer.size,
      e)) {
    goto fail;
  }
  if (!b_question_serialize_to_memory(
      to,
      to_vtable,
      &to_buffer.data,
      &to_buffer.size,
      e)) {
    goto fail;
  }
  return insert_dependency_locked_(
    database,
    from_buffer,
    from_vtable->uuid,
    to_buffer,
    to_vtable->uuid,
    e);

fail:
  if (from_buffer.data) {
    b_deallocate(from_buffer.data);
  }
  if (to_buffer.data) {
    b_deallocate(to_buffer.data);
  }
  return false;
}

static B_WUR B_FUNC bool
insert_dependency_locked_(
    B_BORROW struct B_Database *database,
    B_TRANSFER struct Buffer_ from_data,
    struct B_UUID const from_uuid,
    B_TRANSFER struct Buffer_ to_data,
    struct B_UUID const to_uuid,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(from_data.data);
  B_PRECONDITION(to_data.data);
  B_OUT_PARAMETER(e);

  sqlite3_stmt *stmt = database->insert_dependency_stmt;

  bool ok;

  bool need_free_from_data = true;
  bool need_free_to_data = true;

  ok = bind_uuid_(
    stmt,
    B_INSERT_DEPENDENCY_FROM_QUESTION_UUID,
    from_uuid,
    e);
  if (!ok) goto done_no_reset;

  ok = bind_buffer_(
    stmt,
    B_INSERT_DEPENDENCY_FROM_QUESTION_DATA,
    from_data,
    e);
  need_free_from_data = false;
  if (!ok) goto done_no_reset;

  ok = bind_uuid_(
    stmt,
    B_INSERT_DEPENDENCY_TO_QUESTION_UUID,
    to_uuid,
    e);
  if (!ok) goto done_no_reset;

  ok = bind_buffer_(
    stmt,
    B_INSERT_DEPENDENCY_TO_QUESTION_DATA,
    to_data,
    e);
  need_free_to_data = false;
  if (!ok) goto done_no_reset;

  ok = b_sqlite3_step_expecting_end(stmt, e);
  // TODO(strager): Error reporting.
  (void) sqlite3_reset(stmt);

done_no_reset:
  (void) sqlite3_clear_bindings(stmt);
  if (need_free_from_data) {
    b_deallocate(from_data.data);
  }
  if (need_free_to_data) {
    b_deallocate(to_data.data);
  }
  return ok;
}

static B_WUR B_FUNC bool
look_up_answer_locked_(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_IQuestion const *question,
    B_BORROW struct B_QuestionVTable const *question_vtable,
    B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(question);
  B_PRECONDITION(question_vtable);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  bool ok;
  int rc;

  bool need_free_question_buffer = true;

  struct Buffer_ question_buffer;
  if (!b_question_serialize_to_memory(
      question,
      question_vtable,
      &question_buffer.data,
      &question_buffer.size,
      e)) {
    return false;
  }

  sqlite3_stmt *stmt = database->select_answer_stmt;

  ok = bind_buffer_(
    stmt,
    B_SELECT_ANSWER_QUESTION_DATA,
    question_buffer,
    e);
  need_free_question_buffer = false;
  if (!ok) goto done_no_reset;

  ok = bind_uuid_(
    stmt,
    B_SELECT_ANSWER_QUESTION_UUID,
    question_vtable->uuid,
    e);
  if (!ok) goto done_no_reset;

  // Grab the first row.
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_DONE) {
    // No data.
    *out = NULL;
    ok = true;
    goto done_reset;
  } else if (rc != SQLITE_ROW) {
    B_ASSERT(rc != SQLITE_OK);
    *e = b_sqlite3_error(rc);
    ok = false;
    goto done_reset;
  }

  // Deserialize the answer.
  struct Buffer_ answer_buffer;
  // NOTE(strager): answer_buffer.data is valid until
  // the call to b_sqlite3_step_expecting_end.
  // NOTE(strager): Calls must be performed in this order
  // (sqlite3_column_blob then sqlite3_column_bytes)
  // according to the sqlite3 documentation.
  answer_buffer.data = (void *) sqlite3_column_blob(
    stmt, B_SELECT_ANSWER_ANSWER_DATA);
  if (!answer_buffer.data
      && sqlite3_errcode(database->handle)
        == SQLITE_NOMEM) {
    *e = b_sqlite3_error(SQLITE_NOMEM);
    ok = false;
    goto done_reset;
  }
  answer_buffer.size = sqlite3_column_bytes(
    stmt, B_SELECT_ANSWER_ANSWER_DATA);
  if (!answer_buffer.data) {
    // "The return value from sqlite3_column_blob() for
    // a zero-length BLOB is a NULL pointer."
    B_ASSERT(answer_buffer.size == 0);
  }
  struct B_IAnswer *answer;
  ok = b_answer_deserialize_from_memory(
    question_vtable->answer_vtable,
    answer_buffer.data,
    answer_buffer.size,
    &answer,
    e);
  // Ignore errors, as our lookup is done.
  (void) b_sqlite3_step_expecting_end(
    stmt, &(struct B_Error) {});
  if (!ok) {
    goto done_reset;
  }
  *out = answer;

  // TODO(strager): Error reporting.
done_reset:
  (void) sqlite3_reset(stmt);

done_no_reset:
  (void) sqlite3_clear_bindings(stmt);
  if (need_free_question_buffer) {
    b_deallocate(question_buffer.data);
  }
  return ok;
}

static B_WUR B_FUNC bool
b_check_all_locked_(
    B_BORROW struct B_Database *database,
    B_BORROW struct B_QuestionVTable const *const *vtables,
    size_t vtable_count,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(database);
  B_PRECONDITION(vtables);
  B_OUT_PARAMETER(e);

  database->udf.vtables = vtables;
  database->udf.vtable_count = vtable_count;

  bool ok = b_sqlite3_step_expecting_end(
    database->recheck_all_answers_stmt, e);
  // TODO(strager): Error reporting.
  (void) sqlite3_reset(
    database->recheck_all_answers_stmt);

  database->udf.vtables = NULL;
  database->udf.vtable_count = 0;

  return ok;
}

static B_WUR B_FUNC bool
bind_uuid_(
    B_BORROW sqlite3_stmt *stmt,
    int host_parameter_name,
    struct B_UUID uuid,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(stmt);
  B_PRECONDITION(host_parameter_name > 0);
  B_OUT_PARAMETER(e);

  if (!b_sqlite3_bind_blob(
      stmt,
      host_parameter_name,
      uuid.data,
      sizeof(uuid.data),
      // FIXME(strager): Why doesn't SQLITE_STATIC work?
      SQLITE_TRANSIENT,
      e)) {
    return false;
  }
  return true;
}

static B_WUR B_FUNC bool
bind_buffer_(
    B_BORROW sqlite3_stmt *stmt,
    int host_parameter_name,
    B_TRANSFER struct Buffer_ buffer,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(stmt);
  B_PRECONDITION(host_parameter_name > 0);
  B_PRECONDITION(buffer.data);
  B_OUT_PARAMETER(e);

  if (!b_sqlite3_bind_blob(
      stmt,
      host_parameter_name,
      buffer.data,
      buffer.size,
      deallocate_buffer_data_,
      e)) {
    return false;
  }
  return true;
}

static void
deallocate_buffer_data_(
    B_TRANSFER void *data) {
  b_deallocate(data);
}

// NOTE[b_question_answer_matches]:
// question_answer_matches_locked_ is a UDF in sqlite3 bound
// to b_question_answer_matches.  Its signature is:
//
// b_question_answer_matches(
//   question_uuid BLOB NOT NULL,
//   question_data BLOB NOT NULL,
//   answer_data BLOB NOT NULL) INTEGER NOT NULL
//
// b_question_answer_matches returns 1 if the given answer
// matches the actual answer.  Otherwise, it returns 0.
static void
question_answer_matches_locked_(
    B_BORROW sqlite3_context *context,
    int arg_count,
    B_BORROW sqlite3_value **args) {
  B_PRECONDITION(context);
  B_PRECONDITION(arg_count == 3);
  B_PRECONDITION(args);

  struct B_Database *database = sqlite3_user_data(context);
  B_ASSERT(database);
  B_ASSERT(database->udf.vtables);

  bool result;
  struct B_Error e;
  struct B_QuestionVTable const *question_vtable;
  struct B_IQuestion *question = NULL;
  struct Buffer_ actual_answer_data = {
    .data = NULL,
    .size = 0,
  };

  // Parse arguments.
  struct B_UUID question_uuid;
  if (!value_uuid_(args[0], &question_uuid, &e)) {
    goto fail;
  }
  // NOTE(strager): question_data.data is valid until we
  // touch args again.
  struct Buffer_ question_data;
  if (!b_sqlite3_value_blob(
      args[1],
      (void const **) &question_data.data,
      &question_data.size,
      &e)) {
    goto fail;
  }
  question_vtable = NULL;
  for (size_t i = 0; i < database->udf.vtable_count; ++i) {
    struct B_QuestionVTable const *vtable
      = database->udf.vtables[i];
    if (b_uuid_equal(&vtable->uuid, &question_uuid)) {
      question_vtable = vtable;
      break;
    }
  }
  if (!question_vtable) {
    e = (struct B_Error) {.posix_error = ENOENT};
    goto fail;
  }

  // Deserialize question.
  if (!b_question_deserialize_from_memory(
      question_vtable,
      question_data.data,
      question_data.size,
      &question,
      &e)) {
    goto fail;
  }

  // Get actual answer.
  struct B_IAnswer *answer;
  if (!question_vtable->query_answer(
      question, &answer, &e)) {
    goto fail;
  }
  if (!answer) {
    result = false;
    goto done;
  }
  // question is unused; deallocate.
  (void) question_vtable->deallocate(
    question, &(struct B_Error) {});
  question = NULL;
  if (!b_answer_serialize_to_memory(
      answer,
      question_vtable->answer_vtable,
      &actual_answer_data.data,
      &actual_answer_data.size,
      &e)) {
    goto fail;
  }
  // answer is unused; deallocate.
  (void) question_vtable->answer_vtable->deallocate(
    answer, &(struct B_Error) {});

  // Get stored answer.  Invalidates question_data.
  void const *answer_data;
  size_t answer_data_size;
  if (!b_sqlite3_value_blob(
      args[2], &answer_data, &answer_data_size, &e)) {
    goto fail;
  }
  question_data.data = NULL;

  // Check answer.
  result = (answer_data_size == actual_answer_data.size)
    && (memcmp(
      answer_data,
      actual_answer_data.data,
      answer_data_size) == 0);
  goto done;

done:
  if (actual_answer_data.data) {
    b_deallocate(actual_answer_data.data);
  }
  if (question) {
    B_ASSERT(question_vtable);
    (void) question_vtable->deallocate(
      question, &(struct B_Error) {});
  }

  // TODO(strager): Do something with e.

  sqlite3_result_int(context, result ? 1 : 0);
  return;

fail:
  // FIXME(strager): Should we report an error via
  // sqlite3_result_error instead?
  result = false;
  goto done;
}

static B_WUR B_FUNC bool
value_uuid_(
    B_BORROW sqlite3_value *value,
    B_OUT struct B_UUID *out_uuid,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(value);
  B_OUT_PARAMETER(out_uuid);
  B_OUT_PARAMETER(e);

  // See NOTE[value blob ordering].
  int size = sqlite3_value_bytes(value);
  if (size != sizeof(struct B_UUID)) {
    // TODO(strager): Report data corruption error.
    B_NYI();
    return false;
  }
  void const *data = sqlite3_value_blob(value);
  B_ASSERT(data);
  memcpy(&out_uuid->data, data, sizeof(struct B_UUID));
  return true;
}
