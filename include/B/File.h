#ifndef B_HEADER_GUARD_E2911876_8D19_4C39_97F1_42AA7820356B
#define B_HEADER_GUARD_E2911876_8D19_4C39_97F1_42AA7820356B

#include <B/Base.h>
#include <B/FilePath.h>

struct B_Answer;
struct B_ErrorHandler;
struct B_QuestionVTable;

#if defined(__cplusplus)
extern "C" {
#endif

// QuestionVTable for asking about the contents of a file.
// The given B_Question must be of type
// 'B_FilePath const *'.
//
// Answer-s are derived from the contents, but their values
// are not guaranteed to be stable between versions of B.
//
// If the Question refers to a directory or does not refer
// to a file system entity, an error is returned.
//
// If the Question refers to a symbolic link, the link is
// followed.
struct B_QuestionVTable const *
b_file_contents_question_vtable(
        void);

// QuestionVTable for asking about the list of files in a
// directory.  The given B_Question must be of type
// 'B_FilePath const *'.
//
// Answer-s are derived from the child file system entity
// names only.  Use b_directory_listing_answer_strings to
// inspect an Answer.
//
// If the Question refers to a file or does not refer to a
// file system entity, an error is returned.
//
// If the Question refers to a symbolic link, the link is
// followed.
struct B_QuestionVTable const *
b_directory_listing_question_vtable(
        void);

// Returns the list of entity names discovered through
// b_directory_listing_question_vtable()->answer.
//
// The list is encoded as packed 0-terminated strings (see
// FilePath).  The final string is empty.  For example, for
// a directory containing "app.c" and "app.h", the result
// would be "app.c\0app.h\0\0"
//
// The list does not include the "." and ".." special
// entries.
//
// The list contains relative paths.  Thus, the directory
// separator will not be present in any of the strings.
//
// The list is sorted lexicographically (in binary) so it
// remains stable across calls to answer.
B_EXPORT_FUNC
b_directory_listing_answer_strings(
        struct B_Answer const *,
        B_OUTPTR B_FilePath const **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
