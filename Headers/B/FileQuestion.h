#pragma once

#include <B/Attributes.h>

#include <stdbool.h>

struct B_Error;
struct B_IQuestion;
struct B_QuestionVTable;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_EXPORT_FUNC struct B_QuestionVTable const *
b_file_question_vtable(
    void);

B_WUR B_EXPORT_FUNC bool
b_file_question_allocate(
    B_BORROW char const *file_path,
    B_OUT_TRANSFER struct B_IQuestion **,
    B_OUT struct B_Error *);

B_WUR B_EXPORT_FUNC bool
b_file_question_path(
    B_BORROW struct B_IQuestion const *,
    B_OUT_BORROW char const **,
    B_OUT struct B_Error *);

#if defined(__cplusplus)
}
#endif
