#include <B/Assert.h>
#include <B/Error.h>
#include <B/SQLite3.h>

#include <limits.h>
#include <sqlite3.h>

B_EXPORT_FUNC
b_sqlite3_bind_blob(
        sqlite3_stmt *stmt,
        int host_parameter_name,
        B_TRANSFER void const *data,
        size_t data_size,
        sqlite3_destructor_type destructor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(stmt);
    B_ASSERT(data);

    // FIXME(strager): Should this be a reported error?
    B_ASSERT(data_size <= INT_MAX);

retry:;
    int rc = sqlite3_bind_blob(
        stmt,
        host_parameter_name,
        data,
        data_size,
        destructor);
    if (rc != SQLITE_OK) {
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite3_bind_blob")) {
        case B_ERROR_RETRY:
            goto retry;
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        }
    }
    return true;
}

B_EXPORT_FUNC
b_sqlite3_value_blob(
        sqlite3_value *value,
        B_OUT B_BORROWED void const **out_data,
        B_OUT size_t *out_size,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(value);
    B_ASSERT(out_data);

    // NOTE[value blob ordering]:
    // "Please pay particular attention to the fact that the
    // pointer returned from sqlite3_value_blob(),
    // sqlite3_value_text(), or sqlite3_value_text16() can
    // be invalidated by a subsequent call to
    // sqlite3_value_bytes(), sqlite3_value_bytes16(),
    // sqlite3_value_text(), or sqlite3_value_text16()."
    // FIXME(strager): This contradicts the documentation of
    // sqlite3_column_blob.
    int size = sqlite3_value_bytes(value);
    B_ASSERT(size >= 0);
    *out_size = size;
    if (size == 0) {
        // "The return value from sqlite3_column_blob() for
        // a zero-length BLOB is a NULL pointer."
        // FIXME(strager): I assume sqlite3_value_blob
        // behaves the same way.
        // Various deserialize functions will expect a
        // non-NULL data pointer.
        *out_data = out_data;
    } else {
        *out_data = (void *) sqlite3_value_blob(value);
    }
    return true;
    (void) eh;
}

B_EXPORT_FUNC
b_sqlite3_step_expecting_end(
        sqlite3_stmt *stmt,
        struct B_ErrorHandler const *eh) {
retry:;
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        B_ASSERT(rc != SQLITE_OK);
        B_ASSERT(rc != SQLITE_ROW);
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite3_step")) {
        case B_ERROR_RETRY:
            // TODO(strager): Error reporting.
            goto retry;
        case B_ERROR_ABORT:
            return false;
        case B_ERROR_IGNORE:
            break;
        }
    }
    return true;
}

B_EXPORT_FUNC
b_sqlite3_prepare(
        sqlite3 *handle,
        char const *query,
        size_t query_size,
        B_OUTPTR sqlite3_stmt **out,
        struct B_ErrorHandler const *eh) {
retry:;
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(
        handle,
        query,
        query_size,
        &stmt,
        NULL);
    if (rc != SQLITE_OK) {
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite3_prepare_v2")) {
        case B_ERROR_RETRY:
            goto retry;
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            return false;
        }
    }

    // "If the input text contains no SQL (if the input is
    // an empty string or a comment) then *ppStmt is set to
    // NULL."
    B_ASSERT(stmt);

    *out = stmt;
    return true;
}
