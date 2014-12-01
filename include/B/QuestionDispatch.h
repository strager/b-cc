#ifndef B_HEADER_GUARD_475F7A56_E930_4942_98AA_71778658F8D3
#define B_HEADER_GUARD_475F7A56_E930_4942_98AA_71778658F8D3

#include <B/Base.h>

struct B_AnswerContext;
struct B_Database;
struct B_ErrorHandler;
struct B_QuestionQueue;
struct B_QuestionQueueItem;

#if defined(__cplusplus)
extern "C" {
#endif

// Creates an AnswerContext for the given QuestionQueueItem.
// If the Question in the QuestionQueueItem has already been
// answered according to the given Database, the
// QuestionQueueItem's answer_callback is called with the
// answer and a NULL AnswerContext is returned.
// Non-blocking.
// TODO(strager): Rename this function.
B_EXPORT_FUNC
b_question_dispatch_one(
        B_TRANSFER struct B_QuestionQueueItem *,
        struct B_QuestionQueue *,
        struct B_Database *,
        B_OUTPTR B_OPT struct B_AnswerContext const **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
