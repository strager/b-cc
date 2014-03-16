// NOTE[schema]: There are two tables: dependencies and
// answers.

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Config.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionVTableSet.h>
#include <B/Thread.h>
#include <B/UUID.h>

#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stddef.h>
#include <string.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

// FIXME(strager): _sqlite_error should be used.
#define B_RAISE_SQLITE_ERROR(_eh, _sqlite_error, _context) \
    B_RAISE_ERRNO_ERROR((_eh), ENOTSUP, (_context))

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
#if defined(B_CONFIG_PTHREAD)
    pthread_mutex_t lock;
#endif

    sqlite3 *handle;
    sqlite3_stmt *insert_dependency_stmt;
    sqlite3_stmt *insert_answer_stmt;
    sqlite3_stmt *select_answer_stmt;
    sqlite3_stmt *recheck_all_answers_stmt;

    // Fields for UDFs (User Defined Functions).  Temporary.
    struct {
        struct B_ErrorHandler const *eh;
        struct B_QuestionVTableSet const *question_vtable_set;
    } udf;
};

static B_FUNC
prepare_database_locked_(
        struct B_Database *,
        struct B_ErrorHandler const *);

static B_FUNC
record_answer_locked_(
        struct B_Database *,
        struct B_Question const *,
        struct B_QuestionVTable const *,
        struct B_Answer const *,
        struct B_ErrorHandler const *);

static B_FUNC
insert_answer_locked_(
        struct B_Database *,
        B_TRANSFER struct B_Serialized question_data,
        struct B_UUID const question_uuid,
        B_TRANSFER struct B_Serialized answer_data,
        struct B_ErrorHandler const *);

static B_FUNC
record_dependency_locked_(
        struct B_Database *,
        struct B_Question const *from,
        struct B_QuestionVTable const *from_vtable,
        struct B_Question const *to,
        struct B_QuestionVTable const *to_vtable,
        struct B_ErrorHandler const *);

static B_FUNC
insert_dependency_locked_(
        struct B_Database *,
        B_TRANSFER struct B_Serialized from_data,
        struct B_UUID const from_uuid,
        B_TRANSFER struct B_Serialized to_data,
        struct B_UUID const to_uuid,
        struct B_ErrorHandler const *);

static B_FUNC
look_up_answer_locked_(
        struct B_Database *,
        struct B_Question const *,
        struct B_QuestionVTable const *,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);

static B_FUNC
recheck_all_locked_(
        struct B_Database *,
        struct B_QuestionVTableSet const *,
        struct B_ErrorHandler const *);

static B_FUNC
bind_blob_(
        sqlite3_stmt *,
        int host_parameter_name,
        void const *data,
        size_t data_size,
        sqlite3_destructor_type,
        struct B_ErrorHandler const *);

static B_FUNC
bind_uuid_(
        sqlite3_stmt *,
        int host_parameter_name,
        struct B_UUID,
        struct B_ErrorHandler const *);

static B_FUNC
bind_serialized_(
        sqlite3_stmt *,
        int host_parameter_name,
        B_TRANSFER struct B_Serialized,
        struct B_ErrorHandler const *);

static B_FUNC
prepare_statement_(
        sqlite3 *,
        char const *query,
        size_t query_size,
        B_OUTPTR sqlite3_stmt **,
        struct B_ErrorHandler const *);

static B_FUNC
step_expecting_end_(
        sqlite3_stmt *,
        struct B_ErrorHandler const *);

static void
deallocate_serialized_data_(
        void *data);

static void
question_answer_matches_locked_(
        sqlite3_context *,
        int arg_count,
        sqlite3_value **args);

static B_FUNC
value_blob_(
        sqlite3_value *,
        B_OUT B_BORROWED struct B_Serialized *,
        struct B_ErrorHandler const *);

static B_FUNC
value_uuid_(
        sqlite3_value *,
        B_OUT struct B_UUID *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_database_load_sqlite(
        char const *sqlite_path,
        int sqlite_flags,
        char const *sqlite_vfs,
        B_OUTPTR struct B_Database **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, sqlite_path);
    B_CHECK_PRECONDITION(eh, out);

    // Ensure sqlite3 features used are available.
#define B_REQUIRED_SQLITE_VERSION_NUMBER_ 3008003
#if SQLITE_VERSION_NUMBER < B_REQUIRED_SQLITE_VERSION_NUMBER_
# error "sqlite3 version >=3.8.3 required"
#endif 
    if (sqlite3_libversion_number()
            < B_REQUIRED_SQLITE_VERSION_NUMBER_) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            ENOTSUP,
            "sqlite3_libversion_number");
        return false;
    }
    if (sqlite3_compileoption_used("SQLITE_OMIT_CTE")) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            ENOTSUP,
            "SQLITE_OMIT_CTE");
        return false;
    }

    int rc;

    struct B_Database *database;
    if (!b_allocate(
            sizeof(*database),
            (void **) &database,
            eh)) {
        return false;
    }
    *database = (struct B_Database) {
#if defined(B_CONFIG_PTHREAD)
        .lock = PTHREAD_MUTEX_INITIALIZER,
#endif
        .handle = NULL,
        .insert_dependency_stmt = NULL,
        .insert_answer_stmt = NULL,
        .select_answer_stmt = NULL,
        .recheck_all_answers_stmt = NULL,
        .udf = {
            .eh = NULL,
            .question_vtable_set = NULL,
        },
    };

retry_open:
    rc = sqlite3_open_v2(
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
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite3_open_v2")) {
        case B_ERROR_RETRY:
            goto retry_open;
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        }
    }
    if (!prepare_database_locked_(database, eh)) {
        // NOTE[unprepare database]: database is allowed to
        // be half-initialized.  b_database_close will
        // properly uninitialize only initialized prepared
        // statements.
        goto fail;
    }

    *out = database;
    return true;

fail:
    (void) b_database_close(database, eh);
    return false;
}

B_EXPORT_FUNC
b_database_close(
        B_TRANSFER struct B_Database *database,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, database);

    B_ASSERT(!database->udf.eh);
    B_ASSERT(!database->udf.question_vtable_set);

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
    (void) b_deallocate(database, eh);
    return true;
}

B_EXPORT_FUNC
b_database_record_dependency(
        struct B_Database *database,
        struct B_Question const *from,
        struct B_QuestionVTable const *from_vtable,
        struct B_Question const *to,
        struct B_QuestionVTable const *to_vtable,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, database);
    B_CHECK_PRECONDITION(eh, from);
    B_CHECK_PRECONDITION(eh, from_vtable);
    B_CHECK_PRECONDITION(eh, to);
    B_CHECK_PRECONDITION(eh, to_vtable);

    bool ok = true;
    if (!B_MUTEX_LOCK(database->lock, eh)) return false;
    {
        ok = record_dependency_locked_(
            database,
            from,
            from_vtable,
            to,
            to_vtable,
            eh);
    }
    B_MUTEX_MUST_UNLOCK(database->lock, eh);
    return ok;
}

B_EXPORT_FUNC
b_database_record_answer(
        struct B_Database *database,
        struct B_Question const *question,
        struct B_QuestionVTable const *question_vtable,
        struct B_Answer const *answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, database);
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, question_vtable);
    B_CHECK_PRECONDITION(eh, answer);

    bool ok = true;
    if (!B_MUTEX_LOCK(database->lock, eh)) return false;
    {
        ok = record_answer_locked_(
            database,
            question,
            question_vtable,
            answer,
            eh);
    }
    B_MUTEX_MUST_UNLOCK(database->lock, eh);
    return ok;
}

B_EXPORT_FUNC
b_database_look_up_answer(
        struct B_Database *database,
        struct B_Question const *question,
        struct B_QuestionVTable const *question_vtable,
        B_OUTPTR struct B_Answer **out_answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, database);
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, question_vtable);
    B_CHECK_PRECONDITION(eh, out_answer);

    bool ok = true;
    if (!B_MUTEX_LOCK(database->lock, eh)) return false;
    {
        ok = look_up_answer_locked_(
            database,
            question,
            question_vtable,
            out_answer,
            eh);
    }
    B_MUTEX_MUST_UNLOCK(database->lock, eh);
    return ok;
}

B_EXPORT_FUNC
b_database_recheck_all(
        struct B_Database *database,
        struct B_QuestionVTableSet const *question_vtables,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, database);
    B_CHECK_PRECONDITION(eh, question_vtables);

    bool ok = true;
    if (!B_MUTEX_LOCK(database->lock, eh)) return false;
    {
        ok = recheck_all_locked_(
            database,
            question_vtables,
            eh);
    }
    B_MUTEX_MUST_UNLOCK(database->lock, eh);
    return ok;
}

static B_FUNC
prepare_database_locked_(
        struct B_Database *database,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(database);
    B_ASSERT(database->handle);
    B_ASSERT(!database->insert_dependency_stmt);
    B_ASSERT(!database->insert_answer_stmt);
    B_ASSERT(!database->select_answer_stmt);
    B_ASSERT(!database->recheck_all_answers_stmt);

    sqlite3 *handle = database->handle;
    int rc;

retry_create_function:
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
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite3_create_function_v2")) {
        case B_ERROR_RETRY:
            goto retry_create_function;
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        }
    }

    // See NOTE[schema].
    static char const create_table_query[] = ""
        "CREATE TABLE IF NOT EXISTS dependencies(\n"
        "    from_question_uuid BLOB NOT NULL,\n"
        "    from_question_data BLOB NOT NULL,\n"
        "    to_question_uuid BLOB NOT NULL,\n"
        "    to_question_data BLOB NOT NULL);\n"
        "CREATE TABLE IF NOT EXISTS answers(\n"
        "    question_uuid BLOB NOT NULL,\n"
        "    question_data BLOB NOT NULL,\n"
        "    answer_data BLOB NOT NULL);\n";
retry_create_table:
    rc = sqlite3_exec(
        handle,
        create_table_query,
        NULL,
        NULL,
        NULL);
    if (rc != SQLITE_OK) {
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite_exec")) {
        case B_ERROR_RETRY:
            goto retry_create_table;
        case B_ERROR_ABORT:
        case B_ERROR_IGNORE:
            goto fail;
        }
    }

    // See NOTE[insert dependency query].
    static char const insert_dependency_query[] = ""
        "INSERT INTO dependencies(\n"
        "    from_question_uuid,\n"
        "    from_question_data,\n"
        "    to_question_uuid,\n"
        "    to_question_data)\n"
        "VALUES (?1, ?2, ?3, ?4);";
    if (!prepare_statement_(
            handle,
            insert_dependency_query,
            sizeof(insert_dependency_query),
            &database->insert_dependency_stmt,
            eh)) {
        goto fail;
    }

    // See NOTE[insert answer query].
    static char const insert_answer_query[] = ""
        "INSERT INTO answers(\n"
        "    question_uuid,\n"
        "    question_data,\n"
        "    answer_data)\n"
        "VALUES (?1, ?2, ?3);";
    if (!prepare_statement_(
            handle,
            insert_answer_query,
            sizeof(insert_answer_query),
            &database->insert_answer_stmt,
            eh)) {
        goto fail;
    }

    // See NOTE[select answer query].
    static char const select_answer_query[] = ""
        "SELECT answer_data\n"
        "    FROM answers\n"
        "    WHERE question_uuid = ?1\n"
        "      AND question_data = ?2;";
    if (!prepare_statement_(
            handle,
            select_answer_query,
            sizeof(select_answer_query),
            &database->select_answer_stmt,
            eh)) {
        goto fail;
    }

    // See NOTE[recheck all answers query].
    static char const recheck_all_answers_query[] = ""
        "WITH RECURSIVE invalid_answers(\n"
        "        question_uuid,\n"
        "        question_data) AS (\n"
        "    -- Seed recursion with out-of-date answers."
        "    -- See NOTE[b_question_answer_matches].\n"
        "    SELECT question_uuid, question_data\n"
        "        FROM answers\n"
        "        WHERE b_question_answer_matches(\n"
        "                  question_uuid,\n"
        "                  question_data,\n"
        "                  answer_data) == 0\n"
        "\n"
        "    UNION ALL\n"
        "\n"
        "    -- Walk up the dependency graph.\n"
        "    SELECT dep.from_question_uuid,\n"
        "           dep.from_question_data\n"
        "        FROM invalid_answers AS invalid\n"
        "        INNER JOIN dependencies AS dep\n"
        "        ON dep.to_question_uuid = invalid.question_uuid\n"
        "           AND dep.to_question_data = invalid.question_data\n"
        ")\n"
        "-- Delete encountered rows.\n"
        "DELETE FROM answers WHERE _rowid_ IN (\n"
        "    SELECT answers._rowid_ FROM answers\n"
        "        INNER JOIN invalid_answers AS invalid\n"
        "        ON answers.question_uuid = invalid.question_uuid\n"
        "           AND answers.question_data = invalid.question_data);";
    if (!prepare_statement_(
            handle,
            recheck_all_answers_query,
            sizeof(recheck_all_answers_query),
            &database->recheck_all_answers_stmt,
            eh)) {
        goto fail;
    }

    return true;

fail:
    // See NOTE[unprepare database].
    return false;
}

static B_FUNC
record_answer_locked_(
        struct B_Database *database,
        struct B_Question const *question,
        struct B_QuestionVTable const *question_vtable,
        struct B_Answer const *answer,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(database);

    struct B_Serialized question_serialized = {
        .data = NULL,
        .size = 0,
    };
    struct B_Serialized answer_serialized = {
        .data = NULL,
        .size = 0,
    };

    if (!question_vtable->serialize(
            question,
            &question_serialized,
            eh)) {
        goto fail;
    }
    if (!question_vtable->answer_vtable->serialize(
            answer,
            &answer_serialized,
            eh)) {
        goto fail;
    }

    return insert_answer_locked_(
        database,
        question_serialized,
        question_vtable->uuid,
        answer_serialized,
        eh);

fail:
    if (question_serialized.data) {
        (void) b_deallocate(question_serialized.data, eh);
    }
    if (answer_serialized.data) {
        (void) b_deallocate(answer_serialized.data, eh);
    }
    return false;
}

static B_FUNC
insert_answer_locked_(
        struct B_Database *database,
        B_TRANSFER struct B_Serialized question_data,
        struct B_UUID const question_uuid,
        B_TRANSFER struct B_Serialized answer_data,
        struct B_ErrorHandler const *eh) {
    sqlite3_stmt *stmt = database->insert_answer_stmt;

    bool ok;

    bool need_free_question_data = true;
    bool need_free_answer_data = true;

    ok = bind_serialized_(
        stmt,
        B_INSERT_ANSWER_QUESTION_DATA,
        question_data,
        eh);
    need_free_question_data = false;
    if (!ok) goto done_no_reset;

    ok = bind_uuid_(
        stmt,
        B_INSERT_ANSWER_QUESTION_UUID,
        question_uuid,
        eh);
    if (!ok) goto done_no_reset;

    ok = bind_serialized_(
        stmt,
        B_INSERT_ANSWER_ANSWER_DATA,
        answer_data,
        eh);
    need_free_answer_data = false;
    if (!ok) goto done_no_reset;

    ok = step_expecting_end_(stmt, eh);
    // TODO(strager): Error reporting.
    (void) sqlite3_reset(stmt);

done_no_reset:
    (void) sqlite3_clear_bindings(stmt);
    if (need_free_question_data) {
        (void) b_deallocate(question_data.data, eh);
    }
    if (need_free_answer_data) {
        (void) b_deallocate(answer_data.data, eh);
    }
    return ok;
}

static B_FUNC
record_dependency_locked_(
        struct B_Database *database,
        struct B_Question const *from,
        struct B_QuestionVTable const *from_vtable,
        struct B_Question const *to,
        struct B_QuestionVTable const *to_vtable,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(database);

    struct B_Serialized from_serialized = {
        .data = NULL,
        .size = 0,
    };
    struct B_Serialized to_serialized = {
        .data = NULL,
        .size = 0,
    };

    if (!from_vtable->serialize(
            from,
            &from_serialized,
            eh)) {
        goto fail;
    }
    if (!to_vtable->serialize(to, &to_serialized, eh)) {
        goto fail;
    }

    return insert_dependency_locked_(
        database,
        from_serialized,
        from_vtable->uuid,
        to_serialized,
        to_vtable->uuid,
        eh);

fail:
    if (from_serialized.data) {
        (void) b_deallocate(from_serialized.data, eh);
    }
    if (to_serialized.data) {
        (void) b_deallocate(to_serialized.data, eh);
    }
    return false;
}

static B_FUNC
insert_dependency_locked_(
        struct B_Database *database,
        B_TRANSFER struct B_Serialized from_data,
        struct B_UUID const from_uuid,
        B_TRANSFER struct B_Serialized to_data,
        struct B_UUID const to_uuid,
        struct B_ErrorHandler const *eh) {
    sqlite3_stmt *stmt = database->insert_dependency_stmt;

    bool ok;

    bool need_free_from_data = true;
    bool need_free_to_data = true;

    ok = bind_uuid_(
        stmt,
        B_INSERT_DEPENDENCY_FROM_QUESTION_UUID,
        from_uuid,
        eh);
    if (!ok) goto done_no_reset;

    ok = bind_serialized_(
        stmt,
        B_INSERT_DEPENDENCY_FROM_QUESTION_DATA,
        from_data,
        eh);
    need_free_from_data = false;
    if (!ok) goto done_no_reset;

    ok = bind_uuid_(
        stmt,
        B_INSERT_DEPENDENCY_TO_QUESTION_UUID,
        to_uuid,
        eh);
    if (!ok) goto done_no_reset;

    ok = bind_serialized_(
        stmt,
        B_INSERT_DEPENDENCY_TO_QUESTION_DATA,
        to_data,
        eh);
    need_free_to_data = false;
    if (!ok) goto done_no_reset;

    ok = step_expecting_end_(stmt, eh);
    // TODO(strager): Error reporting.
    (void) sqlite3_reset(stmt);

done_no_reset:
    (void) sqlite3_clear_bindings(stmt);
    if (need_free_from_data) {
        (void) b_deallocate(from_data.data, eh);
    }
    if (need_free_to_data) {
        (void) b_deallocate(to_data.data, eh);
    }
    return ok;
}

static B_FUNC
look_up_answer_locked_(
        struct B_Database *database,
        struct B_Question const *question,
        struct B_QuestionVTable const *question_vtable,
        B_OUTPTR struct B_Answer **out_answer,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(database);

    bool ok;
    int rc;

    bool need_free_question_serialized = true;

    struct B_Serialized question_serialized;
    if (!question_vtable->serialize(
            question,
            &question_serialized,
            eh)) {
        return false;
    }

    sqlite3_stmt *stmt = database->select_answer_stmt;

    ok = bind_serialized_(
        stmt,
        B_SELECT_ANSWER_QUESTION_DATA,
        question_serialized,
        eh);
    need_free_question_serialized = false;
    if (!ok) goto done_no_reset;

    ok = bind_uuid_(
        stmt,
        B_SELECT_ANSWER_QUESTION_UUID,
        question_vtable->uuid,
        eh);
    if (!ok) goto done_no_reset;

    // Grab the first row.
retry_step:
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        // No data.
        *out_answer = NULL;
        ok = true;
        goto done_reset;
    } else if (rc != SQLITE_ROW) {
        B_ASSERT(rc != SQLITE_OK);
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                rc,
                "sqlite3_step")) {
        case B_ERROR_RETRY:
            // NOTE(strager): sqlite3_reset will not reset
            // bound host parameters.
            // TODO(strager): Error reporting.
            (void) sqlite3_reset(stmt);
            goto retry_step;
        case B_ERROR_ABORT:
            ok = false;
            goto done_reset;
        case B_ERROR_IGNORE:
            goto ignore_failure;
        }
    }

    // Deserialize the answer.
    struct B_Serialized answer_serialized;
    // NOTE(strager): answer_serialized.data is valid until
    // the call to step_expecting_end_.
    // NOTE(strager): Calls must be performed in this order
    // (sqlite3_column_blob then sqlite3_column_bytes)
    // according to the sqlite3 documentation.
retry_get_answer_blob:
    answer_serialized.data = (void *) sqlite3_column_blob(
        stmt,
        B_SELECT_ANSWER_ANSWER_DATA);
    if (!answer_serialized.data &&
            sqlite3_errcode(database->handle)
                == SQLITE_NOMEM) {
        switch (B_RAISE_SQLITE_ERROR(
                eh,
                SQLITE_NOMEM,
                "sqlite3_column_blob")) {
        case B_ERROR_RETRY:
            goto retry_get_answer_blob;
        case B_ERROR_ABORT:
            ok = false;
            goto done_reset;
        case B_ERROR_IGNORE:
            goto ignore_failure;
        }
    }
    answer_serialized.size = sqlite3_column_bytes(
        stmt,
        B_SELECT_ANSWER_ANSWER_DATA);
    if (!answer_serialized.data) {
        // "The return value from sqlite3_column_blob() for
        // a zero-length BLOB is a NULL pointer."
        B_ASSERT(answer_serialized.size == 0);
        // AnswerVTable's deserialize function will
        // expect a non-NULL data pointer.
        answer_serialized.data = &answer_serialized.data;
    }
    ok = question_vtable->answer_vtable->deserialize(
        answer_serialized,
        out_answer,
        eh);
    // Ignore errors, as our lookup is done.
    (void) step_expecting_end_(stmt, eh);

    // TODO(strager): Error reporting.
done_reset:
    (void) sqlite3_reset(stmt);

done_no_reset:
    (void) sqlite3_clear_bindings(stmt);
    if (need_free_question_serialized) {
        (void) b_deallocate(question_serialized.data, eh);
    }
    return ok;

ignore_failure:
    ok = true;
    *out_answer = NULL;
    goto done_reset;
}

static B_FUNC
recheck_all_locked_(
        struct B_Database *database,
        struct B_QuestionVTableSet const *question_vtable_set,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(database);
    B_ASSERT(question_vtable_set);

    database->udf.eh = eh;
    database->udf.question_vtable_set = question_vtable_set;

    bool ok = step_expecting_end_(
        database->recheck_all_answers_stmt,
        eh);
    // TODO(strager): Error reporting.
    (void) sqlite3_reset(
        database->recheck_all_answers_stmt);

    database->udf.eh = NULL;
    database->udf.question_vtable_set = NULL;

    return ok;
}

static B_FUNC
bind_blob_(
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

static B_FUNC
bind_uuid_(
        sqlite3_stmt *stmt,
        int host_parameter_name,
        struct B_UUID uuid,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(stmt);
    return bind_blob_(
        stmt,
        host_parameter_name,
        uuid.data,
        sizeof(uuid.data),
        // FIXME(strager): Why doesn't SQLITE_STATIC work?
        SQLITE_TRANSIENT,
        eh);
}

static B_FUNC
bind_serialized_(
        sqlite3_stmt *stmt,
        int host_parameter_name,
        B_TRANSFER struct B_Serialized serialized,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(stmt);
    return bind_blob_(
        stmt,
        host_parameter_name,
        serialized.data,
        serialized.size,
        deallocate_serialized_data_,
        eh);
}

static B_FUNC
prepare_statement_(
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

static B_FUNC
step_expecting_end_(
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

static void
deallocate_serialized_data_(
        void *data) {
    (void) b_deallocate(data, NULL);
}

// NOTE[b_question_answer_matches]:
// question_answer_matches_locked_ is a UDF in sqlite3 bound
// to b_question_answer_matches.  Its signature is:
//
// b_question_answer_matches(
//     question_uuid BLOB NOT NULL,
//     question_data BLOB NOT NULL,
//     answer_data BLOB NOT NULL) INTEGER NOT NULL
//
// b_question_answer_matches returns 1 if the given answer
// matches the actual answer.  Otherwise, it returns 0.
static void
question_answer_matches_locked_(
        sqlite3_context *context,
        int arg_count,
        sqlite3_value **args) {
    bool result;
    struct B_QuestionVTable const *question_vtable;
    struct B_Question *question = NULL;
    struct B_Serialized actual_answer_data = {
        .data = NULL,
        .size = 0,
    };

    struct B_Database *database
        = sqlite3_user_data(context);
    B_ASSERT(database);
    B_ASSERT(arg_count == 3);
    B_ASSERT(database->udf.question_vtable_set);

    struct B_ErrorHandler const *eh = database->udf.eh;

    // Parse arguments.
    struct B_UUID question_uuid;
    if (!value_uuid_(args[0], &question_uuid, eh)) {
        goto fail;
    }
    // NOTE(strager): question_data.data is valid until we
    // touch args again.
    struct B_Serialized question_data;
    if (!value_blob_(args[1], &question_data, eh)) {
        goto fail;
    }
    if (!b_question_vtable_set_look_up(
            database->udf.question_vtable_set,
            question_uuid,
            &question_vtable,
            eh)) {
        goto fail;
    }

    // Deserialize question.
    if (!question_vtable->deserialize(
            question_data,
            &question,
            eh)) {
        goto fail;
    }

    // Get actual answer.
    struct B_Answer *answer;
    if (!question_vtable->answer(question, &answer, eh)) {
        goto fail;
    }
    if (!question_vtable->answer_vtable->serialize(
            answer,
            &actual_answer_data,
            eh)) {
        goto fail;
    }

    // question is unused; deallocate.
    (void) question_vtable->deallocate(question, eh);
    question = NULL;

    // Get stored answer.  Invalidates question_data.
    struct B_Serialized answer_data;
    if (!value_blob_(args[2], &answer_data, eh)) {
        goto fail;
    }
    question_data.data = NULL;

    // Check answer.
    result = (answer_data.size == actual_answer_data.size)
        && (memcmp(
            answer_data.data,
            actual_answer_data.data,
            answer_data.size) == 0);
    goto done;

done:
    if (actual_answer_data.data) {
        (void) b_deallocate(actual_answer_data.data, eh);
    }
    if (question) {
        B_ASSERT(question_vtable);
        (void) question_vtable->deallocate(question, eh);
    }

    sqlite3_result_int(context, result ? 1 : 0);
    return;

fail:
    // FIXME(strager): Should we report an error via
    // sqlite3_result_error instead?
    result = false;
    goto done;
}

static B_FUNC
value_blob_(
        sqlite3_value *value,
        B_OUT B_BORROWED struct B_Serialized *out_data,
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
    out_data->size = size;
    if (size == 0) {
        // "The return value from sqlite3_column_blob() for
        // a zero-length BLOB is a NULL pointer."
        // FIXME(strager): I assume sqlite3_value_blob
        // behaves the same way.
        // Various deserialize functions will expect a
        // non-NULL data pointer.
        out_data->data = &out_data->data;
    } else {
        out_data->data = (void *) sqlite3_value_blob(value);
    }
    return true;
    (void) eh;
}

static B_FUNC
value_uuid_(
        sqlite3_value *value,
        B_OUT struct B_UUID *out_uuid,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(value);
    B_ASSERT(out_uuid);

    // See NOTE[value blob ordering].
    int size = sqlite3_value_bytes(value);
    if (size != sizeof(struct B_UUID)) {
        // TODO(strager): Report data corruption error.
        (void) eh;
        return false;
    }
    void const *data = sqlite3_value_blob(value);
    B_ASSERT(data);
    memcpy(&out_uuid->data, data, sizeof(struct B_UUID));
    return true;
}
