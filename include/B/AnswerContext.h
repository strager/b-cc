#ifndef B_HEADER_GUARD_438F0290_9427_4311_A2AF_6FD486A4BBC5
#define B_HEADER_GUARD_438F0290_9427_4311_A2AF_6FD486A4BBC5

#include <B/Base.h>
#include <B/QuestionAnswer.h>

#include <stddef.h>

struct B_Database;
struct B_ErrorHandler;
struct B_ProcessController;
struct B_Question;
struct B_QuestionQueue;
struct B_QuestionVTable;

// An AnswerContext is given to a Question's answer
// function.  The answer function is expected to call
// b_answer_context_success with the Answer, or
// b_answer_context_error.  Progress can be reported via
// b_answer_context_details.
//
// When dependencies are expressed using an AnswerContext,
// the dependency callback is enqueued into the owning
// QuestionQueue.
//
// Thread-safe: YES
// Signal-safe: NO
struct B_AnswerContext;

typedef B_FUNC
B_NeedCompletedCallback(
        // Transfers ownership of the Answer-s, but not the
        // array of Answer-s.
        B_TRANSFER struct B_Answer *const *,
        void *opaque,
        struct B_ErrorHandler const *);

typedef B_FUNC
B_NeedCancelledCallback(
        void *opaque,
        struct B_ErrorHandler const *);

struct B_AnswerContext {
    // The question being asked.  Used when calling
    // answer_callback.
    B_BORROWED struct B_Question const *question;
    B_BORROWED struct B_QuestionVTable const *question_vtable;

    // Called by b_answer_context_success or
    // b_answer_context_error.
    B_BORROWED B_AnswerCallback *answer_callback;
    B_BORROWED void *answer_callback_opaque;

    // b_question_queue_enqueue called by
    // b_answer_context_need.
    B_BORROWED struct B_QuestionQueue *question_queue;

    // b_database_record_dependency called by
    // b_answer_context_need.
    B_BORROWED B_OPT struct B_Database *database;
};

#if defined(__cplusplus)
extern "C" {
#endif

// Expresses a dependency from the Question being answered
// to the given Question-s.  When this function returns, the
// given Question-s are not necessarily answered; instead,
// either the completed_callback or cancelled_callback is
// called later .
//
// If questions_answers is non-NULL, it is populated with
// owning pointers to Answer-s before calling
// completed_callback.
//
// If completed_callback is called, it will not be called
// again, and cancelled_callback will not be called.
//
// If cancelled_callback is called, it will not be called
// again, and completed_callback will not be called.
//
// Callbacks are typically called on the thread
// b_question_dispatch is called on.
//
// May be called more than once, even with the same
// Question-s.
B_EXPORT_FUNC
b_answer_context_need(
        struct B_AnswerContext const *,
        struct B_Question const *const *questions,
        struct B_QuestionVTable const *const *questions_vtables,
        size_t questions_count,
        B_NeedCompletedCallback *completed_callback,
        B_NeedCancelledCallback *cancelled_callback,
        void *callback_opaque,
        struct B_ErrorHandler const *);

// Like b_answer_context_success_answer, but gathering the
// answer from a call to QuestionVTable::answer.
B_EXPORT_FUNC
b_answer_context_success(
        struct B_AnswerContext const *,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_answer_context_success_answer(
        B_TRANSFER struct B_AnswerContext const *,
        B_TRANSFER struct B_Answer *,
        struct B_ErrorHandler const *);

// TODO(strager): Error code?
B_EXPORT_FUNC
b_answer_context_error(
        B_TRANSFER struct B_AnswerContext const *,
        struct B_ErrorHandler const *);

// TODO(strager)
//B_EXPORT_FUNC
//b_answer_context_details(
//        struct B_AnswerContext const *,
//        struct B_ErrorHandler const *);

// Convenience function which calls
// b_process_controller_exec_basic and (eventually)
// b_answer_context_success (upon an exit code of zero) or
// b_answer_context_error (upon a non-zero exit code,
// signal, or exception).
B_EXPORT_FUNC
b_answer_context_exec(
        struct B_AnswerContext const *,
        struct B_ProcessController *,
        char const *const *args,
        struct B_ErrorHandler const *);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
# include <B/Alloc.h>
# include <B/Config.h>

// Closure for b_answer_context_need.
template<
        typename TCompletedCallback,
        typename TCancelledCallback>
struct B_NeedCallbacks {
    typedef B_FUNC
    Completed(
            B_TRANSFER struct B_Answer *const *,
            B_ErrorHandler const *);

    typedef B_FUNC
    Cancelled(
            B_ErrorHandler const *);

    B_NeedCallbacks(
            TCompletedCallback completed_callback,
            TCancelledCallback cancelled_callback) :
            completed_callback(completed_callback),
            cancelled_callback(cancelled_callback) {
    }

    TCompletedCallback completed_callback;
    TCancelledCallback cancelled_callback;

    static B_FUNC
    completed(
            B_TRANSFER struct B_Answer *const *answers,
            void *opaque,
            B_ErrorHandler const *eh) {
        auto *self = static_cast<B_NeedCallbacks *>(opaque);
        if (!self) {
            (void) eh;  // TODO(strager)
        }
        if (!self->completed_callback(answers, eh)) {
            return false;
        }
        self->~B_NeedCallbacks();
        (void) b_deallocate(self, eh);
        return true;
    }

    static B_FUNC
    cancelled(void *opaque, B_ErrorHandler const *eh) {
        auto *self = static_cast<B_NeedCallbacks *>(
                opaque);
        if (!self) {
            (void) eh;  // TODO(strager)
        }
        if (!self->cancelled_callback(eh)) {
            return false;
        }
        self->~B_NeedCallbacks();
        (void) b_deallocate(self, eh);
        return true;
    }
};

template<
        typename TCompletedCallback,
        typename TCancelledCallback>
B_FUNC
b_answer_context_need(
        B_AnswerContext const *answer_context,
        B_Question const *const *questions,
        B_QuestionVTable const *const *questions_vtables,
        size_t questions_count,
        TCompletedCallback completed_callback,
        TCancelledCallback cancelled_callback,
        B_ErrorHandler const *eh) {
    B_NeedCallbacks<
        TCompletedCallback, TCancelledCallback> *callbacks;
    if (!b_allocate(
            sizeof(*callbacks),
            reinterpret_cast<void **>(&callbacks),
            eh)) {
        return false;
    }
    new (callbacks) B_NeedCallbacks<
            TCompletedCallback, TCancelledCallback>(
        static_cast<TCompletedCallback &&>(
            completed_callback),
        static_cast<TCancelledCallback &&>(
            cancelled_callback));

    // TODO(strager): Deallocate callbacks on failure.
    return b_answer_context_need(
        answer_context,
        questions,
        questions_vtables,
        questions_count,
        callbacks->completed,
        callbacks->cancelled,
        callbacks,
        eh);
}

template<
        typename TCompletedCallback,
        typename TCancelledCallback>
B_FUNC
b_answer_context_need_one(
        B_AnswerContext const *answer_context,
        B_Question const *question,
        B_QuestionVTable const *question_vtable,
        TCompletedCallback const &completed_callback,
        TCancelledCallback const &cancelled_callback,
        B_ErrorHandler const *eh) {
    B_Question const *const *questions = &question;
    B_QuestionVTable const *const *questions_vtables
        = &question_vtable;
    size_t questions_count = 1;

    return b_answer_context_need(
        answer_context,
        questions,
        questions_vtables,
        questions_count,
        [completed_callback](
                B_TRANSFER B_Answer *const *answers,
                B_ErrorHandler const *eh) {
            return completed_callback(answers[0], eh);
        },
        cancelled_callback,
        eh);
}
#endif

#endif
