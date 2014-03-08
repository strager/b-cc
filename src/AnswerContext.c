#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/DependencyDelegate.h>
#include <B/Errno.h>
#include <B/Error.h>
#include <B/Process.h>
#include <B/QuestionQueue.h>
#include <B/RefCount.h>

#include <stdbool.h>
#include <stdlib.h>

#define B_CHECK_PRECONDITION_ANSWER_CONTEXT(_eh, _answer_context) \
    do { \
        B_CHECK_PRECONDITION((_eh), (_answer_context)); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->question); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->question_vtable); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->answer_callback); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->question_queue); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->dependency_delegate); \
    } while (0)

#define B_CHECK_PRECONDITION_NEED_CLOSURE(_eh, _need_closure) \
    do { \
        B_CHECK_PRECONDITION((_eh), (_need_closure)->questions_count > 0); \
        B_CHECK_PRECONDITION((_eh), (_need_closure)->completed_callback); \
        B_CHECK_PRECONDITION((_eh), (_need_closure)->cancelled_callback); \
        B_CHECK_PRECONDITION((_eh), (_need_closure)->answered_questions_count <= (_need_closure)->questions_count); \
        if ((_need_closure)->answered_questions_count == (_need_closure)->questions_count) { \
            B_CHECK_PRECONDITION((_eh), (_need_closure)->callback_called); \
        } \
    } while (0)

struct NeedClosure_ {
    B_REF_COUNTED_OBJECT;

    B_BORROWED struct B_AnswerContext const *B_CONST_STRUCT_MEMBER answer_context;

    size_t B_CONST_STRUCT_MEMBER questions_count;

    B_NeedCompletedCallback *B_CONST_STRUCT_MEMBER completed_callback;
    B_NeedCancelledCallback *B_CONST_STRUCT_MEMBER cancelled_callback;
    B_BORROWED void *B_CONST_STRUCT_MEMBER callback_opaque;

    // If answered_questions_count == questions_count,
    // completed_callback was called.  Else,
    // cancelled_callback was called.
    bool callback_called;

    // Immutable after callback_called == true.
    size_t answered_questions_count;

    // Must be last.  Access using
    // need_closure_question_vtable_ and
    // need_closure_question_answer_.
    //B_BORRWED struct B_QuestionVTable questions_vtables[questions_count];
    //struct B_Answer *questions_answers[questions_count];
};

struct NeedQueueItem_ {
    struct B_QuestionQueueItemObject super;
    B_BORROWED struct NeedClosure_ *B_CONST_STRUCT_MEMBER closure;
    size_t question_index;
};

B_STATIC_ASSERT(
    offsetof(struct NeedQueueItem_, super) == 0,
    "NeedQueueItem_::super must be the first member");

static B_FUNC
need_one_(
        struct B_AnswerContext const *,
        struct B_Question const *,
        struct B_QuestionVTable const *,
        size_t question_index,
        struct NeedClosure_ *,
        struct B_ErrorHandler const *);

static B_FUNC
need_queue_item_deallocate_(
        struct B_QuestionQueueItemObject *,
        struct B_ErrorHandler const *);

static B_FUNC
need_closure_allocate_(
        struct B_AnswerContext const *,
        struct B_QuestionVTable const *const *questions_vtables,
        size_t questions_count,
        B_NeedCompletedCallback *completed_callback,
        B_NeedCancelledCallback *cancelled_callback,
        void *callback_opaque,
        B_OUTPTR struct NeedClosure_ **,
        struct B_ErrorHandler const *);

static B_FUNC
need_closure_release_(
        struct NeedClosure_ *,
        struct B_ErrorHandler const *);

static B_FUNC
need_closure_complete_(
        struct NeedClosure_ *,
        struct B_ErrorHandler const *);

static B_FUNC
need_closure_cancel_(
        struct NeedClosure_ *,
        struct B_ErrorHandler const *);

static struct B_QuestionVTable const **
need_closure_questions_vtables_(
        struct NeedClosure_ *);

static struct B_Answer **
need_closure_questions_answers_(
        struct NeedClosure_ *);

static B_FUNC
need_answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *,
        void *opaque,
        struct B_ErrorHandler const *);

static B_FUNC
exec_exit_callback_(
        int exit_code,
        void *opaque,
        struct B_ErrorHandler const *);

static B_FUNC
exec_error_callback_(
        struct B_Error,
        void *opaque,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_answer_context_need(
        struct B_AnswerContext const *answer_context,
        struct B_Question const *const *questions,
        struct B_QuestionVTable const *const *questions_vtables,
        size_t questions_count,
        B_NeedCompletedCallback *completed_callback,
        B_NeedCancelledCallback *cancelled_callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);
    B_CHECK_PRECONDITION(eh, answer_context->dependency_delegate);
    B_CHECK_PRECONDITION(eh, answer_context->dependency_delegate->dependency);
    B_CHECK_PRECONDITION(eh, questions);
    B_CHECK_PRECONDITION(eh, questions_vtables);
    B_CHECK_PRECONDITION(eh, completed_callback);
    B_CHECK_PRECONDITION(eh, cancelled_callback);
    for (size_t i = 0; i < questions_count; ++i) {
        B_CHECK_PRECONDITION(eh, questions[i]);
        B_CHECK_PRECONDITION(eh, questions_vtables[i]);
    }

    if (questions_count == 0) {
        // Must be done.  For questions_count > 0, we rely
        // upon question_queue calling
        // need_answer_callback_ which will call the
        // callback.
        struct B_Answer *dummy_question_answer;
        return completed_callback(
                &dummy_question_answer,
                callback_opaque,
                eh);
    }

    // TODO(strager): As an optimization, if all questions
    // are answered, we can avoid going through the question
    // queue and call completed_callback immediately.

    struct NeedClosure_ *need_closure = NULL;
    bool item_enqueued = false;

    if (!need_closure_allocate_(
            answer_context,
            questions_vtables,
            questions_count,
            completed_callback,
            cancelled_callback,
            callback_opaque,
            &need_closure,
            eh)) {
        goto fail;
    }

    // FIXME(strager): If we've enqueued at least one
    // question, and an error occurs when enqueueing the
    // remaining questions, we are kind of stuck: we can't
    // dequeue enqueued questions.  The best we can do (for
    // now) is call cancelled_callback so answers to
    // enqueued questions are ignored.
    for (size_t i = 0; i < questions_count; ++i) {
        if (!need_one_(
                answer_context,
                questions[i],
                questions_vtables[i],
                i,
                need_closure,
                eh)) {
            goto fail;
        }
        item_enqueued = true;
    }
    if (!need_closure_release_(need_closure, eh)) {
        return false;
    }
    return true;

fail:
    if (item_enqueued) {
        (void) need_closure_cancel_(callback_opaque, eh);
    }
    if (need_closure) {
        // Pretend we called the cancelled callback.
        need_closure->callback_called = true;
        (void) need_closure_release_(need_closure, eh);
    }
    return false;
}

static B_FUNC
need_one_(
        struct B_AnswerContext const *answer_context,
        struct B_Question const *question,
        struct B_QuestionVTable const *question_vtable,
        size_t question_index,
        struct NeedClosure_ *need_closure,
        struct B_ErrorHandler const *eh) {
    struct B_Question *question_replica = NULL;
    struct NeedQueueItem_ *queue_item = NULL;
    bool release_need_closure = false;

    if (!B_RETAIN(need_closure, eh)) {
        goto fail;
    }
    release_need_closure = true;

    if (!answer_context->dependency_delegate->dependency(
                answer_context->dependency_delegate,
                answer_context->question,
                answer_context->question_vtable,
                question,
                question_vtable,
                eh)) {
        goto fail;
    }
    if (!question_vtable->replicate(
            question,
            &question_replica,
            eh)) {
        goto fail;
    }
    if (!b_allocate(
            sizeof(*queue_item),
            (void **) &queue_item,
            eh)) {
        goto fail;
    }
    *queue_item = (struct NeedQueueItem_) {
        .super = {
            .deallocate = need_queue_item_deallocate_,
            .question = question_replica,
            .question_vtable = question_vtable,
            .answer_callback = need_answer_callback_,
        },
        .closure = need_closure,
        .question_index = question_index,
    };
    release_need_closure = false;
    if (!b_question_queue_enqueue(
            answer_context->question_queue,
            &queue_item->super,
            eh)) {
        goto fail;
    }
    return true;

fail:
    if (question_replica) {
        (void) question_vtable->deallocate(
                question_replica,
                eh);
    }
    if (queue_item) {
        (void) b_question_queue_item_object_deallocate(
                &queue_item->super,
                eh);
    }
    if (release_need_closure) {
        (void) need_closure_release_(need_closure, eh);
    }
    return false;
}

B_EXPORT_FUNC
b_answer_context_success(
        struct B_AnswerContext const *answer_context,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);

    struct B_Answer *answer;
    if (!answer_context->question_vtable->answer(
            answer_context->question,
            &answer,
            eh)) {
        return false;
    }

    return b_answer_context_success_answer(
        answer_context,
        answer,
        eh);
}

B_EXPORT_FUNC
b_answer_context_success_answer(
        struct B_AnswerContext const *answer_context,
        B_TRANSFER struct B_Answer *answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);
    B_CHECK_PRECONDITION(eh, answer);

    return answer_context->answer_callback(
            answer,
            answer_context->answer_callback_opaque,
            eh);
}

B_EXPORT_FUNC
b_answer_context_error(
        struct B_AnswerContext const *answer_context,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);

    // FIXME(strager): Ugly!  Error code or message would be
    // nicer.
    struct B_Answer *answer = NULL;

    return answer_context->answer_callback(
            answer,
            answer_context->answer_callback_opaque,
            eh);
}

B_EXPORT_FUNC
b_answer_context_exec(
        struct B_AnswerContext const *answer_context,
        struct B_ProcessLoop *process_loop,
        char const *const *args,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);
    B_CHECK_PRECONDITION(eh, process_loop);
    B_CHECK_PRECONDITION(eh, args);

    return b_process_loop_exec(
        process_loop,
        args,
        exec_exit_callback_,
        exec_error_callback_,
        (void *) answer_context,
        eh);
}

static B_FUNC
need_queue_item_deallocate_(
        struct B_QuestionQueueItemObject *queue_item,
        struct B_ErrorHandler const *eh) {
    struct NeedQueueItem_ *need_queue_item
            = (struct NeedQueueItem_ *) queue_item;
    if (!queue_item->question_vtable->deallocate(
            queue_item->question,
            eh)) {
        return false;
    }

    (void) need_closure_release_(
        need_queue_item->closure,
        eh);
    (void) b_deallocate(need_queue_item, eh);
    return true;
}

static B_FUNC
need_closure_allocate_(
        struct B_AnswerContext const *answer_context,
        struct B_QuestionVTable const *const *questions_vtables,
        size_t questions_count,
        B_NeedCompletedCallback *completed_callback,
        B_NeedCancelledCallback *cancelled_callback,
        void *callback_opaque,
        B_OUTPTR struct NeedClosure_ **out,
        struct B_ErrorHandler const *eh) {
    struct NeedClosure_ *need_closure = NULL;

    size_t need_closure_size = sizeof(struct NeedClosure_)
        + (sizeof(struct B_QuestionVTable *)
            * questions_count)
        + (sizeof(struct B_Answer *) * questions_count);
    if (!b_allocate(
            need_closure_size,
            (void **) &need_closure,
            eh)) {
        goto fail;
    }
    *need_closure = (struct NeedClosure_) {
        .answer_context = answer_context,
        .questions_count = questions_count,
        .completed_callback = completed_callback,
        .cancelled_callback = cancelled_callback,
        .callback_opaque = callback_opaque,
        .callback_called = false,
        .answered_questions_count = 0,
    };
    struct B_QuestionVTable const **out_questions_vtables
        = need_closure_questions_vtables_(need_closure);
    struct B_Answer **out_questions_answers
        = need_closure_questions_answers_(need_closure);
    for (size_t i = 0; i < questions_count; ++i) {
        out_questions_vtables[i] = questions_vtables[i];
        out_questions_answers[i] = NULL;
    }
    if (!B_REF_COUNTED_OBJECT_INIT(need_closure, eh)) {
        goto fail;
    }

    *out = need_closure;
    return true;

fail:
    if (need_closure) {
        (void) b_deallocate(need_closure, eh);
    }
    return false;
}

static B_FUNC
need_closure_release_(
        struct NeedClosure_ *need_closure,
        struct B_ErrorHandler const *eh) {
    bool should_deallocate;
    if (!B_RELEASE(need_closure, &should_deallocate, eh)) {
        return false;
    }
    if (!should_deallocate) {
        return true;
    }

    B_ASSERT(need_closure->callback_called);
    for (
            size_t i = 0;
            i < need_closure->questions_count;
            ++i) {
        struct B_Answer *answer
            = need_closure_questions_answers_(need_closure)
                [i];
        if (answer) {
            struct B_AnswerVTable const *answer_vtable
                = need_closure_questions_vtables_(
                    need_closure)[i]->answer_vtable;
            (void) answer_vtable->deallocate(answer, eh);
        }
    }
    (void) b_deallocate(need_closure, eh);
    return true;
}

static B_FUNC
need_closure_complete_(
        struct NeedClosure_ *need_closure,
        struct B_ErrorHandler const *eh) {
    if (need_closure->callback_called) {
        if (need_closure->answered_questions_count
                == need_closure->questions_count) {
            // completed_callback already called.
            enum B_ErrorHandlerResult result
                = B_RAISE_ERRNO_ERROR(
                    eh,
                    B_ERROR_ALREADY_COMPLETED,
                    "already completed");
            switch (result) {
            case B_ERROR_ABORT:
            case B_ERROR_RETRY:
                return false;
            case B_ERROR_IGNORE:
                return true;
            }
        } else {
            // cancelled_callback already called.
            enum B_ErrorHandlerResult result
                = B_RAISE_ERRNO_ERROR(
                    eh,
                    B_ERROR_ALREADY_CANCELLED,
                    "previously cancelled");
            switch (result) {
            case B_ERROR_ABORT:
            case B_ERROR_RETRY:
                return false;
            case B_ERROR_IGNORE:
                return true;
            }
        }
    }

    need_closure->callback_called = true;
    return need_closure->completed_callback(
            need_closure_questions_answers_(need_closure),
            need_closure->callback_opaque,
            eh);
}

static B_FUNC
need_closure_cancel_(
        struct NeedClosure_ *need_closure,
        struct B_ErrorHandler const *eh) {
    if (need_closure->callback_called) {
        if (need_closure->answered_questions_count
                == need_closure->questions_count) {
            // completed_callback already called.
            enum B_ErrorHandlerResult result
                = B_RAISE_ERRNO_ERROR(
                    eh,
                    B_ERROR_ALREADY_COMPLETED,
                    "previously completed");
            switch (result) {
            case B_ERROR_ABORT:
            case B_ERROR_RETRY:
                return false;
            case B_ERROR_IGNORE:
                return true;
            }
        } else {
            // cancelled_callback already called.
            return true;
        }
    }

    need_closure->callback_called = true;
    if (!need_closure->cancelled_callback(
            need_closure->callback_opaque,
            eh)) {
        // An error occured, but we don't want to deadlock
        // the queue, so keep going.
    }
    struct B_Answer *answer = NULL;  // FIXME(strager)
    struct B_AnswerContext const *answer_context
            = need_closure->answer_context;
    if (!answer_context->answer_callback(
            answer,
            answer_context->answer_callback_opaque,
            eh)) {
        return false;
    }

    return true;
}

static struct B_QuestionVTable const **
need_closure_questions_vtables_(
        struct NeedClosure_ *need_closure) {
    B_ASSERT(need_closure);
    return (struct B_QuestionVTable const **)
        ((char *) need_closure + sizeof(*need_closure));
}

static struct B_Answer **
need_closure_questions_answers_(
        struct NeedClosure_ *need_closure) {
    B_ASSERT(need_closure);
    size_t question_vtables_size
        = sizeof(struct B_QuestionVTable *)
        * need_closure->questions_count;
    return (struct B_Answer **)
        ((char *) need_closure + sizeof(*need_closure)
            + question_vtables_size);
}

static B_FUNC
need_answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *answer,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, opaque);

    struct NeedQueueItem_ *need_queue_item = opaque;
    struct NeedClosure_ *need_closure
            = need_queue_item->closure;
    size_t question_index = need_queue_item->question_index;

    B_CHECK_PRECONDITION_NEED_CLOSURE(eh, need_closure);
    B_CHECK_PRECONDITION(eh, question_index
        < need_closure->questions_count);
    B_CHECK_PRECONDITION(eh,
        !need_closure_questions_answers_(need_closure)
            [question_index]);

    if (answer) {
        need_closure_questions_answers_(need_closure)
            [question_index] = answer;
        need_closure->answered_questions_count += 1;
        if (need_closure->answered_questions_count
                == need_closure->questions_count) {
            return need_closure_complete_(need_closure, eh);
        } else {
            return true;
        }
    } else {
        return need_closure_cancel_(need_closure, eh);
    }
}

static B_FUNC
exec_exit_callback_(
        int exit_code,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    struct B_AnswerContext const *answer_context
        = (const void *) opaque;
    B_CHECK_PRECONDITION(eh, answer_context);

    if (exit_code == 0) {
        return b_answer_context_success(answer_context, eh);
    } else {
        return b_answer_context_error(answer_context, eh);
    }
}

static B_FUNC
exec_error_callback_(
        struct B_Error error,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    struct B_AnswerContext const *answer_context
        = (const void *) opaque;
    B_CHECK_PRECONDITION(eh, answer_context);

    (void) error;  // TODO(strager)
    return b_answer_context_error(answer_context, eh);
}
