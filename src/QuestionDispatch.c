#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/DependencyDelegate.h>
#include <B/Error.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>

#include <stdlib.h>

struct AnswerCallbackClosure_ {
    struct B_AnswerContext *answer_context;
    struct B_QuestionQueueItemObject *queue_item;
};

static B_FUNC
answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *,
        void *opaque,
        struct B_ErrorHandler const *);

static B_FUNC
dispatch_one_(
        struct B_QuestionQueue *question_queue,
        struct B_DependencyDelegateObject *dependency_delegate,
        QuestionDispatchCallback *callback,
        void *callback_opaque,
        bool *keep_going,
        struct B_ErrorHandler const *eh);

B_EXPORT_FUNC
b_question_dispatch(
        struct B_QuestionQueue *question_queue,
        struct B_DependencyDelegateObject *dependency_delegate,
        QuestionDispatchCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question_queue);
    B_CHECK_PRECONDITION(eh, dependency_delegate);
    B_CHECK_PRECONDITION(eh, dependency_delegate->dependency);
    B_CHECK_PRECONDITION(eh, callback);

    bool keep_going = true;
    while (keep_going) {
        if (!dispatch_one_(
                question_queue,
                dependency_delegate,
                callback,
                callback_opaque,
                &keep_going,
                eh)) {
            return false;
        }
    }
    return true;
}

static B_FUNC
dispatch_one_(
        struct B_QuestionQueue *question_queue,
        struct B_DependencyDelegateObject *dependency_delegate,
        QuestionDispatchCallback *callback,
        void *callback_opaque,
        bool *keep_going,
        struct B_ErrorHandler const *eh) {
    struct AnswerCallbackClosure_ *closure = NULL;
    struct B_QuestionQueueItemObject *queue_item = NULL;
    struct B_AnswerContext *answer_context = NULL;

    // Perform allocations before dequeueing to front-load
    // errors.
    if (!b_allocate(
            sizeof(*closure),
            (void **) &closure,
            eh)) {
        goto fail;
    }
    if (!b_allocate(
            sizeof(*answer_context),
            (void **) &answer_context,
            eh)) {
        goto fail;
    }

    if (!b_question_queue_dequeue(
            question_queue,
            &queue_item,
            eh)) {
        goto fail;
    }
    if (!queue_item) {
        (void) b_deallocate(closure, eh);
        (void) b_deallocate(answer_context, eh);
        *keep_going = false;
        return true;
    }
    B_ASSERT(queue_item->question);
    B_ASSERT(queue_item->question_vtable);
    B_ASSERT(queue_item->answer_callback);

    *closure = (struct AnswerCallbackClosure_) {
        .answer_context = answer_context,
        .queue_item = queue_item,
    };
    *answer_context = (struct B_AnswerContext) {
        .question = queue_item->question,
        .question_vtable = queue_item->question_vtable,
        .answer_callback = answer_callback_,
        .answer_callback_opaque = closure,
        .question_queue = question_queue,
        .dependency_delegate = dependency_delegate,
    };

    if (!callback(
            answer_context,
            callback_opaque,
            eh)) {
        goto fail;
    }

    *keep_going = true;
    return true;

fail:
    if (answer_context) {
        (void) b_deallocate(
                answer_context,
                eh);
    }
    if (queue_item) {
        // FIXME(strager): Should we re-enqueue instead?
        (void) b_question_queue_item_object_deallocate(
                queue_item,
                eh);
    }
    if (closure) {
        (void) b_deallocate(
                closure,
                eh);
    }
    return false;
}

static B_FUNC
answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *answer,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    struct AnswerCallbackClosure_ *closure = opaque;
    B_CHECK_PRECONDITION(eh, closure);
    B_CHECK_PRECONDITION(eh, closure->queue_item);
    B_CHECK_PRECONDITION(eh, closure->queue_item->answer_callback);

    if (!closure->queue_item->answer_callback(
        answer,
        closure->queue_item,
        eh)) {
        return false;
    }

    (void) b_deallocate(closure->answer_context, eh);
    (void) b_question_queue_item_object_deallocate(
        closure->queue_item,
        eh);
    (void) b_deallocate(closure, eh);
    return true;
}
