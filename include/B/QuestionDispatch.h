#ifndef B_HEADER_GUARD_475F7A56_E930_4942_98AA_71778658F8D3
#define B_HEADER_GUARD_475F7A56_E930_4942_98AA_71778658F8D3

#include <B/Base.h>

struct B_AnswerContext;
struct B_Database;
struct B_ErrorHandler;
struct B_Question;
struct B_QuestionQueue;
struct B_QuestionVTable;

typedef B_FUNC
B_QuestionDispatchCallback(
        struct B_AnswerContext const *,
        void *opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
extern "C" {
#endif

// Dequeues items from the given QuestionQueue, answering
// questions by calling the given QuestionCallback with a
// corresponding AnswerContext.  Consults a Database for
// previously-determined Answers and writes dependencies and
// answers to the Database.
B_EXPORT_FUNC
b_question_dispatch(
        struct B_QuestionQueue *,
        struct B_Database *,
        B_QuestionDispatchCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#endif
