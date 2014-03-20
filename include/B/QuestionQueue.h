#ifndef B_HEADER_GUARD_7049CA04_7328_408F_9C05_63A469B24B8A
#define B_HEADER_GUARD_7049CA04_7328_408F_9C05_63A469B24B8A

#include <B/Base.h>
#include <B/QuestionAnswer.h>

struct B_ErrorHandler;

// An unordered, thread-safe, dequeue-blocking queue of
// Question-s.  Producers create QuestionQueueItemObject-s
// and add them to the queue with b_question_queue_enqueue
// in a fire-and-forget manner.  Consumers call
// b_question_queue_dequeue and call answer_callback on
// returned QuestionQueueItemObject-s.
//
// Generally, your application's consumer will be
// b_question_dispatch and your application's producer will
// be b_answer_context_need.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_QuestionQueue;
struct B_QuestionQueueItemObject;

#if defined(__cplusplus)
extern "C" {
#endif

B_EXPORT_FUNC
b_question_queue_allocate(
        B_OUTPTR struct B_QuestionQueue **,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_queue_deallocate(
        struct B_QuestionQueue *,
        struct B_ErrorHandler const *);

// Non-blocking.  Does nothing if b_question_queue_close was
// called.
B_EXPORT_FUNC
b_question_queue_enqueue(
        struct B_QuestionQueue *,
        B_TRANSFER struct B_QuestionQueueItemObject *,
        struct B_ErrorHandler const *);

// Blocking.  Outputs a NULL pointer if
// b_question_queue_close was called.
B_EXPORT_FUNC
b_question_queue_dequeue(
        struct B_QuestionQueue *,
        B_OUTPTR struct B_QuestionQueueItemObject *B_OPT *,
        struct B_ErrorHandler const *);

// Non-blocking.  Outputs a NULL pointer if
// b_question_queue_close was called or if the queue is
// empty.
B_EXPORT_FUNC
b_question_queue_try_dequeue(
        struct B_QuestionQueue *,
        B_OUTPTR struct B_QuestionQueueItemObject *B_OPT *,
        struct B_ErrorHandler const *);

// After this is called, b_question_queue_dequeue will
// return NULL, and b_question_queue_enqueue will do
// nothing.  Idempotent.
B_EXPORT_FUNC
b_question_queue_close(
        struct B_QuestionQueue *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_queue_item_object_deallocate(
        struct B_QuestionQueueItemObject *,
        struct B_ErrorHandler const *);

B_ABSTRACT struct B_QuestionQueueItemObject {
    B_FUNC
    (*deallocate)(
            struct B_QuestionQueueItemObject *,
            struct B_ErrorHandler const *);

    // Ownership decided by ::deallocate.
    struct B_Question *question;

    B_BORROWED struct B_QuestionVTable const *question_vtable;

    // opaque points to the QuestionQueueItem.
    B_AnswerCallback *answer_callback;
};

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/CXX.h>

struct B_QuestionQueueDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(
            B_QuestionQueue *queue) {
        (void) b_question_queue_deallocate(
            queue,
            this->error_handler);
    }
};

struct B_QuestionQueueItemDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(
            B_QuestionQueueItemObject *queue_item) {
        (void) b_question_queue_item_object_deallocate(
            queue_item,
            this->error_handler);
    }
};
#endif

#endif
