#ifndef B_HEADER_GUARD_FDE6CC99_8E79_4216_A100_9C8329C4368F
#define B_HEADER_GUARD_FDE6CC99_8E79_4216_A100_9C8329C4368F

#include <B/Base.h>

#include <errno.h>
#include <sqlite3.h>
#include <stddef.h>

// FIXME(strager): _sqlite_error should be used.
#define B_RAISE_SQLITE_ERROR(_eh, _sqlite_error, _context) \
    B_RAISE_ERRNO_ERROR((_eh), ENOTSUP, (_context))

struct B_ErrorHandler;

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_sqlite3_bind_blob(
        sqlite3_stmt *,
        int host_parameter_name,
        void const *data,
        size_t data_size,
        sqlite3_destructor_type,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_sqlite3_value_blob(
        sqlite3_value *,
        B_OUT B_BORROWED void const **out_data,
        B_OUT size_t *out_size,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_sqlite3_step_expecting_end(
        sqlite3_stmt *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_sqlite3_prepare(
        sqlite3 *,
        char const *query,
        size_t query_size,
        B_OUTPTR sqlite3_stmt **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
