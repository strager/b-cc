#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/DependencyDelegate.h>
#include <B/Errno.h>
#include <B/Error.h>
#include <B/Private/RefCount.h>
#include <B/Process.h>
#include <B/QuestionQueue.h>

#include <stdbool.h>
#include <stdlib.h>

#define B_CHECK_PRECONDITION_ANSWER_CONTEXT(_eh, _answer_context) \
    do { \
        B_CHECK_PRECONDITION((_eh), (_answer_context)); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->question); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->question_vtable); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->answer_callback); \
        B_CHECK_PRECONDITION((_eh), (_answer_context)->question_queue); \
        if ((_answer_context)->dependency_delegate) { \
            B_CHECK_PRECONDITION((_eh), (_answer_context)->dependency_delegate->dependency); \
        } \
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

struct NeedClosure_;

struct NeedQueueItem_ {
    struct B_QuestionQueueItemObject super;
    B_BORROWED struct NeedClosure_ *B_CONST_STRUCT_MEMBER closure;
    struct B_Answer *answer;
};

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

    // Variadic; must be last.
    B_BORROWED struct NeedQueueItem_ queue_items[];
};

B_STATIC_ASSERT(
    offsetof(struct NeedQueueItem_, super) == 0,
    "NeedQueueItem_::super must be the first member");

static B_FUNC
need_one_(
        struct B_AnswerContext const *,
        struct NeedQueueItem_ *,
        struct B_ErrorHandler const *);

static B_FUNC
need_queue_item_deallocate_(
        struct B_QuestionQueueItemObject *,
        struct B_ErrorHandler const *);

static B_FUNC
need_closure_allocate_(
        struct B_AnswerContext const *,
        struct B_Question const *const *questions,
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
            &dummy_question_answer, callback_opaque, eh);
    }

    // TODO(strager): As an optimization, if all questions
    // are answered, we can avoid going through the question
    // queue and call completed_callback immediately.

    struct NeedClosure_ *need_closure = NULL;
    bool item_enqueued = false;

    if (!need_closure_allocate_(
            answer_context,
            questions,
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
                &need_closure->queue_items[i],
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
        struct NeedQueueItem_ *queue_item,
        struct B_ErrorHandler const *eh) {
    if (answer_context->dependency_delegate) {
        if (!answer_context->dependency_delegate
                ->dependency(
                    answer_context->dependency_delegate,
                    answer_context->question,
                    answer_context->question_vtable,
                    queue_item->super.question,
                    queue_item->super.question_vtable,
                    eh)) {
            goto fail;
        }
    }

    // When we enqueue the NeedQueueItem, we must retain the
    // NeedClosure (since the NeedQueueItem is embedded in
    // the NeedClosure).
    if (!B_RETAIN(queue_item->closure, eh)) {
        goto fail;
    }
    if (!b_question_queue_enqueue(
            answer_context->question_queue,
            &queue_item->super,
            eh)) {
        (void) need_closure_release_(
            queue_item->closure, eh);
        goto fail;
    }

    return true;

fail:;
    return false;
}

B_EXPORT_FUNC
b_answer_context_success(
        struct B_AnswerContext const *answer_context,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);

    struct B_Answer *answer;
    if (!answer_context->question_vtable->answer(
            answer_context->question, &answer, eh)) {
        return false;
    }

    return b_answer_context_success_answer(
        answer_context, answer, eh);
}

B_EXPORT_FUNC
b_answer_context_success_answer(
        struct B_AnswerContext const *answer_context,
        B_TRANSFER struct B_Answer *answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION_ANSWER_CONTEXT(eh, answer_context);
    B_CHECK_PRECONDITION(eh, answer);

    return answer_context->answer_callback(
        answer, answer_context->answer_callback_opaque, eh);
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
        answer, answer_context->answer_callback_opaque, eh);
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
    // FIXME(strager): This disagrees with the comment in
    // AnswerContext.h.
    if (!queue_item->question_vtable->deallocate(
            queue_item->question, eh)) {
        return false;
    }

    (void) need_closure_release_(
        need_queue_item->closure, eh);
    return true;
}

static B_FUNC
need_closure_allocate_(
        struct B_AnswerContext const *answer_context,
        struct B_Question const *const *questions,
        struct B_QuestionVTable const *const *questions_vtables,
        size_t questions_count,
        B_NeedCompletedCallback *completed_callback,
        B_NeedCancelledCallback *cancelled_callback,
        void *callback_opaque,
        B_OUTPTR struct NeedClosure_ **out,
        struct B_ErrorHandler const *eh) {
    struct NeedClosure_ *need_closure = NULL;
    size_t replicas_made = 0;

    size_t need_closure_size = sizeof(struct NeedClosure_)
        + (sizeof(struct NeedQueueItem_) * questions_count);
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
    for (
            ;
            replicas_made < questions_count;
            ++replicas_made) {
        size_t i = replicas_made;
        struct B_Question *question_replica;
        if (!questions_vtables[i]->replicate(
                questions[i], &question_replica, eh)) {
            goto fail;
        }
        need_closure->queue_items[i]
            = (struct NeedQueueItem_) {
                .super = {
                    .deallocate
                        = need_queue_item_deallocate_,
                    .question = question_replica,
                    .question_vtable = questions_vtables[i],
                    .answer_callback
                        = need_answer_callback_,
                },
                .closure = need_closure,
                .answer = NULL,
            };
    }
    if (!B_REF_COUNTED_OBJECT_INIT(need_closure, eh)) {
        goto fail;
    }

    *out = need_closure;
    return true;

fail:
    if (need_closure) {
        for (size_t i = 0; i < replicas_made; ++i) {
            // Deallocate the replicas we've made.
            (void) questions_vtables[i]->deallocate(
                need_closure->queue_items[i].super.question,
                eh);
        }
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

    if (!need_closure->callback_called) {
        // Ensure the callback is called so we don't leak
        // resources.
        (void) need_closure_cancel_(need_closure, eh);
    }
    B_ASSERT(need_closure->callback_called);

    for (
            size_t i = 0;
            i < need_closure->questions_count;
            ++i) {
        struct NeedQueueItem_ *queue_item
            = &need_closure->queue_items[i];
        struct B_Answer *answer = queue_item->answer;
        if (answer) {
            struct B_AnswerVTable const *answer_vtable
                = queue_item->super.question_vtable
                    ->answer_vtable;
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
    bool ok;

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

    size_t questions_count = need_closure->questions_count;
    struct B_Answer **answers = NULL;
    if (!b_allocate(
            sizeof(struct B_Answer *) * questions_count,
            (void **) &answers,
            eh)) {
        goto fail;
    }
    for (size_t i = 0; i < questions_count; ++i) {
        answers[i] = need_closure->queue_items[i].answer;
    }

    need_closure->callback_called = true;
    if (!need_closure->completed_callback(
            answers, need_closure->callback_opaque, eh)) {
        goto fail;
    }

    ok = true;
done:
    if (answers) {
        (void) b_deallocate(answers, eh);
    }
    return ok;

fail:
    ok = false;
    goto done;
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
            need_closure->callback_opaque, eh)) {
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

static B_FUNC
need_answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *answer,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, opaque);

    struct NeedQueueItem_ *need_queue_item = opaque;
    struct NeedClosure_ *need_closure
        = need_queue_item->closure;

    B_CHECK_PRECONDITION_NEED_CLOSURE(eh, need_closure);
    B_CHECK_PRECONDITION(eh, !need_queue_item->answer);

    if (answer) {
        need_queue_item->answer = answer;
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
