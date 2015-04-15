#pragma once

#include <B/Attributes.h>

#include <stdbool.h>

struct B_Error;
struct B_IAnswer;
struct B_IQuestion;
struct B_QuestionVTable;

struct B_Database;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC bool
b_database_record_dependency(
    B_BORROW struct B_Database *,
    B_BORROW struct B_IQuestion const *from,
    B_BORROW struct B_QuestionVTable const *from_vtable,
    B_BORROW struct B_IQuestion const *to,
    B_BORROW struct B_QuestionVTable const *to_vtable,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_database_record_answer(
    B_BORROW struct B_Database *,
    B_BORROW struct B_IQuestion const *,
    B_BORROW struct B_QuestionVTable const *,
    B_BORROW struct B_IAnswer const *,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_database_look_up_answer(
    B_BORROW struct B_Database *,
    B_BORROW struct B_IQuestion const *,
    B_BORROW struct B_QuestionVTable const *,
    B_OPTIONAL_OUT_TRANSFER struct B_IAnswer **,
    B_OUT struct B_Error *);

// For more methods, see <B/Database.h>.

#if defined(__cplusplus)
}
#endif
