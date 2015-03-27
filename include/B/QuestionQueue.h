#ifndef B_HEADER_GUARD_7049CA04_7328_408F_9C05_63A469B24B8A
#define B_HEADER_GUARD_7049CA04_7328_408F_9C05_63A469B24B8A

#include <B/Base.h>
#include <B/QuestionAnswer.h>

#include <stdbool.h>

struct B_ErrorHandler;

// An unordered, thread-safe, non-blocking queue of
// Question-s.  Producers create QuestionQueueItem-s
// (including a Question and a callback) and add them to the
// queue with b_question_queue_enqueue in a fire-and-forget
// manner.  Consumers call b_question_queue_try_dequeue and
// call answer_callback on returned QuestionQueueItem-s.
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

// Creates a QuestionQueue which will not produce any signal
// when b_question_queue_enqueue is called.
B_EXPORT_FUNC
b_question_queue_allocate_single_threaded(
        B_OUTPTR struct B_QuestionQueue **,
        struct B_ErrorHandler const *);

// Creates a QuestionQueue which will signal the given Linux
// eventfd when b_question_queue_enqueue is called.  Calling
// b_question_queue_try_dequeue does not remove the event
// from the eventfd.
B_EXPORT_FUNC
b_question_queue_allocate_with_eventfd(
        B_BORROWED int eventfd,
        B_OUTPTR struct B_QuestionQueue **,
        struct B_ErrorHandler const *);

// Creates a QuestionQueue which will call kevent with the
// given changelist on the given kqueue file descriptor when
// b_question_queue_enqueue is called.  Calling
// b_question_queue_try_dequeue does not clear the event in
// the kqueue.
B_EXPORT_FUNC
b_question_queue_allocate_with_kqueue(
        B_BORROWED int kqueue_fd,
        B_BORROWED const void *trigger,  // kevent
        B_OUTPTR struct B_QuestionQueue **,
        struct B_ErrorHandler const *);

// Creates a QuestionQueue which will call kevent64 with the
// given changelist on the given kqueue file descriptor when
// b_question_queue_enqueue is called.  Calling
// b_question_queue_try_dequeue does not clear the event in
// the kqueue.
B_EXPORT_FUNC
b_question_queue_allocate_with_kqueue64(
        B_BORROWED int kqueue_fd,
        B_BORROWED const void *trigger,  // kevent64_s
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

// Pops a QuestionQueueItem enqueued by
// b_question_queue_enqueue not already returned by a
// previous call to b_question_queue_try_dequeue.
//
// If b_question_queue_close was called on the given
// QuestionQueue, returns a NULL
// question queue item and returns true for closed.
//
// If no QuestionQueueItem was enqueued (i.e. if the queue
// is empty), returns a NULL question queue item and returns
// false for closed.
//
// Otherwise, transfers ownership of a QuestionQueueItem to
// the caller and returns false for closed.
//
// This function does not block waiting for a
// QuestionQueueItem to be enqueued.
B_EXPORT_FUNC
b_question_queue_try_dequeue(
        struct B_QuestionQueue *,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *,
        B_OUT bool *closed,
        struct B_ErrorHandler const *);

// After this is called, b_question_queue_try_dequeue will
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

#endif
