#ifndef B_HEADER_GUARD_9D1DA759_E302_4651_A64C_06178991A2A9
#define B_HEADER_GUARD_9D1DA759_E302_4651_A64C_06178991A2A9

#include <B/Base.h>

struct B_Answer;
struct B_ErrorHandler;
struct B_Question;
struct B_QuestionVTable;
struct B_QuestionVTableSet;

// A connection to a sqlite3 database.
//
// Questions are dirtied if the stored answer is not known
// to be correct.  For example, if a question was about the
// state of the filesystem, and the filesystem possibly
// changed between database loads, that question should be
// dirtied.  False-positives are acceptable; they will just
// reduce database performance.  False-negatives are not
// acceptable; they will compromise the correctness of a
// build.
//
// NOTE(strager): Question dirtying is not implemented.
//
// Thread-safe: YES if SQLITE_THREADSAFE
// Signal-safe: NO
struct B_Database;

#if defined(__cplusplus)
extern "C" {
#endif

// See sqlite_open_v2 for details on the sqlite_path,
// sqlite_flags, and sqlite_vfs parameters.
B_EXPORT_FUNC
b_database_load_sqlite(
        char const *sqlite_path,
        int sqlite_flags,
        char const *sqlite_vfs,
        B_OUTPTR struct B_Database **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_database_close(
        B_TRANSFER struct B_Database *,
        struct B_ErrorHandler const *);

// Writes the existence of a dependency between two
// questions.
B_EXPORT_FUNC
b_database_record_dependency(
        struct B_Database *,
        struct B_Question const *from,
        struct B_QuestionVTable const *from_vtable,
        struct B_Question const *to,
        struct B_QuestionVTable const *to_vtable,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_database_record_answer(
        struct B_Database *,
        struct B_Question const *,
        struct B_QuestionVTable const *,
        struct B_Answer const *,
        struct B_ErrorHandler const *);

// Finds the answer to a question.  If the question was
// never previously answered, NULL is returned.
B_EXPORT_FUNC
b_database_look_up_answer(
        struct B_Database *,
        struct B_Question const *,
        struct B_QuestionVTable const *,
        B_OUTPTR struct B_Answer **,
        struct B_ErrorHandler const *);

#if 0
// TODO(strager)
B_EXPORT_FUNC
b_database_dirty_answer(
        struct B_Database *,
        struct B_Question const *,
        struct B_QuestionVTable const *,
        struct B_ErrorHandler const *);
#endif

// Immediately rechecks all answers.  When answers are shown
// to no longer be correct, that answer and answers to
// dependent questions are invalidated.
//
// QuestionVTable and AnswerVTable callbacks must *not*
// touch the Database.  Doing so may result in a deadlock or
// corrupt database.
B_EXPORT_FUNC
b_database_recheck_all(
        struct B_Database *,
        struct B_QuestionVTableSet const *,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/CXX.h>

struct B_DatabaseDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(B_Database *database) {
        (void) b_database_close(
            database, this->error_handler);
    }
};
#endif

#endif
