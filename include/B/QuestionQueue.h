#ifndef B_HEADER_GUARD_7049CA04_7328_408F_9C05_63A469B24B8A
#define B_HEADER_GUARD_7049CA04_7328_408F_9C05_63A469B24B8A

#include <B/Base.h>
#include <B/QuestionAnswer.h>

struct B_ErrorHandler;

// An unordered, thread-safe, dequeue-blocking queue of
// Question-s.  Producers create QuestionQueueItem-s and add
// them to the queue with b_question_queue_enqueue in a
// fire-and-forget manner.  Consumers call
// b_question_queue_dequeue and call answer_callback on
// returned QuestionQueueItem-s.
//
// Generally, your application's consumer will immediately
// call b_question_dispatch_one and your application's
// producer will be b_answer_context_need.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_QuestionQueue;
struct B_QuestionQueueItem;
struct B_QuestionQueueRoot;

#if defined(__cplusplus)
extern "C" {
#endif

// Creates a QuestionQueue which will cause
// b_question_queue_dequeue to spin block on an empty queue.
B_EXPORT_FUNC
b_question_queue_allocate_single_threaded(
        B_OUTPTR struct B_QuestionQueue **,
        struct B_ErrorHandler const *);

// Creates a QuestionQueue which will signal the given
// pthread_cond_t when b_question_queue_enqueue is called.
B_EXPORT_FUNC
b_question_queue_allocate_with_pthread_cond(
        B_BORROWED void *pthread_cond,  // pthread_cond_t *
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
        B_TRANSFER struct B_QuestionQueueItem *,
        struct B_ErrorHandler const *);

// Blocking.  Outputs a NULL pointer if
// b_question_queue_close was called.
B_EXPORT_FUNC
b_question_queue_dequeue(
        struct B_QuestionQueue *,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *,
        struct B_ErrorHandler const *);

// Non-blocking.  Outputs a NULL pointer if
// b_question_queue_close was called or if the queue is
// empty.
B_EXPORT_FUNC
b_question_queue_try_dequeue(
        struct B_QuestionQueue *,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *,
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
        struct B_QuestionQueueItem *,
        struct B_ErrorHandler const *);

B_ABSTRACT struct B_QuestionQueueItem {
    B_FUNC
    (*deallocate)(
            struct B_QuestionQueueItem *,
            struct B_ErrorHandler const *);

    // Ownership decided by ::deallocate.
    struct B_Question *question;

    B_BORROWED struct B_QuestionVTable const *question_vtable;

    // opaque points to the QuestionQueueItem.
    B_AnswerCallback *answer_callback;
};

// Enqueues a QuestionQueueItem which, when answered, will
// close the QuestionQueue.  Use
// b_question_queue_finalize_root to retreive the Answer and
// clean up resources.
B_EXPORT_FUNC
b_question_queue_enqueue_root(
        B_BORROWED struct B_QuestionQueue *,
        B_BORROWED const struct B_Question *,
        B_BORROWED const struct B_QuestionVTable *,
        B_OUTPTR struct B_QuestionQueueRoot **,
        struct B_ErrorHandler const *);

// Deallocates a QuestionQueueRoot and returns the Answer to
// the Question given to b_question_queue_enqueue_root.
B_EXPORT_FUNC
b_question_queue_finalize_root(
        B_TRANSFER struct B_QuestionQueueRoot *,
        B_OUTPTR B_TRANSFER struct B_Answer **,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/CXX.h>

struct B_QuestionQueueDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(B_QuestionQueue *queue) {
        (void) b_question_queue_deallocate(
            queue, this->error_handler);
    }
};

struct B_QuestionQueueItemDeleter :
        public B_Deleter {
    using B_Deleter::B_Deleter;

    void
    operator()(B_QuestionQueueItem *queue_item) {
        (void) b_question_queue_item_object_deallocate(
            queue_item, this->error_handler);
    }
};
#endif

#endif
