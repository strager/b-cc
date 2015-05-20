#include <B/Error.h>
#include <B/Private/Assertions.h>
#include <B/Private/Log.h>
#include <B/Private/SQLite3.h>

#include <limits.h>
#include <sqlite3.h>

B_WUR B_FUNC struct B_Error
b_sqlite3_error(
    int sqlite_rc) {
  (void) sqlite_rc;
  B_NYI();
  return (struct B_Error) {.posix_error = 0};
}

B_WUR B_EXPORT_FUNC bool
b_sqlite3_bind_blob(
    B_BORROW sqlite3_stmt *stmt,
    int host_parameter_name,
    B_TRANSFER void const *data,
    size_t data_size,
    B_BORROW sqlite3_destructor_type destructor,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(stmt);
  B_PRECONDITION(host_parameter_name > 0);
  B_PRECONDITION(destructor);
  B_OUT_PARAMETER(e);

  // FIXME(strager): Should this be a reported error?
  B_ASSERT(data_size <= INT_MAX);
  int rc = sqlite3_bind_blob(
    stmt,
    host_parameter_name,
    data,
    (int) data_size,
    destructor);
  if (rc != SQLITE_OK) {
    *e = b_sqlite3_error(rc);
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_sqlite3_value_blob(
    B_BORROW sqlite3_value *value,
    B_OUT_BORROW void const **out_data,
    B_OUT size_t *out_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(value);
  B_OUT_PARAMETER(out_size);
  B_OUT_PARAMETER(out_data);
  B_OUT_PARAMETER(e);

  // NOTE[value blob ordering]:
  // "Please pay particular attention to the fact that the
  // pointer returned from sqlite3_value_blob(),
  // sqlite3_value_text(), or sqlite3_value_text16() can be
  // invalidated by a subsequent call to
  // sqlite3_value_bytes(), sqlite3_value_bytes16(),
  // sqlite3_value_text(), or sqlite3_value_text16()."
  // FIXME(strager): This contradicts the documentation of
  // sqlite3_column_blob.
  int size = sqlite3_value_bytes(value);
  B_ASSERT(size >= 0);
  *out_size = (size_t) size;
  if (size == 0) {
    // "The return value from sqlite3_column_blob() for a
    // zero-length BLOB is a NULL pointer."
    // FIXME(strager): I assume sqlite3_value_blob behaves
    // the same way.
    // Various deserialize functions will expect a non-NULL
    // data pointer.
    *out_data = out_data;
  } else {
    *out_data = (void *) sqlite3_value_blob(value);
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_sqlite3_step_expecting_end(
    B_BORROW sqlite3_stmt *stmt,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(stmt);
  B_OUT_PARAMETER(e);

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    B_ASSERT(rc != SQLITE_OK);
    B_ASSERT(rc != SQLITE_ROW);
    *e = b_sqlite3_error(rc);
    return false;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_sqlite3_prepare(
    B_BORROW sqlite3 *handle,
    B_BORROW char const *query,
    size_t query_size,
    B_OUT_TRANSFER sqlite3_stmt **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(handle);
  B_PRECONDITION(query);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  sqlite3_stmt *stmt;
  // FIXME(strager): Should this be a reported error?
  B_ASSERT(query_size <= INT_MAX);
  int rc = sqlite3_prepare_v2(
    handle, query, (int) query_size, &stmt, NULL);
  if (rc != SQLITE_OK) {
    *e = b_sqlite3_error(rc);
    return false;
  }

  // "If the input text contains no SQL (if the input is an
  // empty string or a comment) then *ppStmt is set to
  // NULL."
  B_ASSERT(stmt);

  *out = stmt;
  return true;
}
