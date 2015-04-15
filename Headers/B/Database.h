#pragma once

#include <B/Attributes.h>

#include <stdbool.h>
#include <stddef.h>

struct B_Error;
struct B_QuestionVTable;

struct B_Database;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_database_open_sqlite3(
    B_BORROW char const *sqlite_path,
    int sqlite_flags,
    B_BORROW_OPTIONAL char const *sqlite_vfs,
    B_OUT_TRANSFER struct B_Database **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_database_close(
    B_TRANSFER struct B_Database *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_database_check_all(
    B_BORROW struct B_Database *,
    B_BORROW struct B_QuestionVTable const *const *,
    size_t question_vtable_count,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
