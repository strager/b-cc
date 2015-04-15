#pragma once

#include <B/Attributes.h>

#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>

struct B_Error;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC struct B_Error
b_sqlite3_error(
    int sqlite_rc);

B_WUR B_EXPORT_FUNC bool
b_sqlite3_bind_blob(
    B_BORROW sqlite3_stmt *,
    int host_parameter_name,
    B_BORROW void const *data,
    size_t data_size,
    B_BORROW sqlite3_destructor_type,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_sqlite3_value_blob(
    B_BORROW sqlite3_value *,
    B_OUT_BORROW void const **out_data,
    B_OUT size_t *out_size,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_sqlite3_step_expecting_end(
    B_BORROW sqlite3_stmt *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_sqlite3_prepare(
    B_BORROW sqlite3 *,
    B_BORROW char const *query,
    size_t query_size,
    B_OUT_TRANSFER sqlite3_stmt **,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
