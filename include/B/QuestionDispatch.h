#ifndef B_HEADER_GUARD_475F7A56_E930_4942_98AA_71778658F8D3
#define B_HEADER_GUARD_475F7A56_E930_4942_98AA_71778658F8D3

#include <B/Base.h>

struct B_AnswerContext;
struct B_Database;
struct B_ErrorHandler;
struct B_Question;
struct B_QuestionQueue;
struct B_QuestionQueueItem;
struct B_QuestionVTable;

typedef B_FUNC
B_QuestionDispatchCallback(
        struct B_AnswerContext const *,
        void *opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
extern "C" {
#endif

// Asynchronously answers the Question in the given
// QueueItem by calling the given QuestionCallback with an
// AnswerContext corresponding to the QueueItem.  Consults a
// Database for previously-determined Answers and writes
// dependencies and answers to the Database.  Enqueues new
// questions onto the QuestionQueue if answering the given
// Question requires answering other questions.
// Non-blocking.
B_EXPORT_FUNC
b_question_dispatch_one(
        B_TRANSFER struct B_QuestionQueueItem *,
        struct B_QuestionQueue *,
        struct B_Database *,
        B_QuestionDispatchCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Alloc.h>
# include <B/Assert.h>

template<typename TDispatchCallback>
B_FUNC
b_question_dispatch_one(
        B_TRANSFER struct B_QuestionQueueItem *queue_item,
        struct B_QuestionQueue *queue,
        struct B_Database *database,
        TDispatchCallback callback,
        struct B_ErrorHandler const *eh) {
    TDispatchCallback *heap_callback = nullptr;

    if (!b_new(&heap_callback, eh, std::move(callback))) {
        goto fail;
    }

    B_QuestionDispatchCallback *real_callback;
    real_callback = [](
            B_AnswerContext const *answer_context,
            void *opaque,
            B_ErrorHandler const *eh) -> bool {
        auto heap_callback
            = static_cast<TDispatchCallback *>(opaque);
        bool ok = (*heap_callback)(answer_context, eh);
        (void) b_delete(heap_callback, eh);
        return ok;
    };

    if (!b_question_dispatch_one(
            queue_item,
            queue,
            database,
            real_callback,
            heap_callback,
            eh)) {
        goto fail;
    }

    return true;

fail:
    if (heap_callback) {
        (void) b_delete(heap_callback, eh);
    }
    return false;
}
#endif

#endif
