#include <B/Alloc.h>
#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>

#include <stdlib.h>

struct QuestionDispatchClosure_ {
    struct B_AnswerContext answer_context;
    struct B_QuestionQueueItemObject *B_CONST_STRUCT_MEMBER queue_item;
    struct B_Database *B_CONST_STRUCT_MEMBER database;
};

static B_FUNC
answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *,
        void *opaque,
        struct B_ErrorHandler const *);

static B_FUNC
dispatch_one_(
        struct B_QuestionQueue *question_queue,
        struct B_Database *database,
        B_QuestionDispatchCallback *callback,
        void *callback_opaque,
        bool *keep_going,
        struct B_ErrorHandler const *eh);

B_EXPORT_FUNC
b_question_dispatch(
        struct B_QuestionQueue *question_queue,
        struct B_Database *database,
        B_QuestionDispatchCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, question_queue);
    B_CHECK_PRECONDITION(eh, database);
    B_CHECK_PRECONDITION(eh, callback);

    bool keep_going = true;
    while (keep_going) {
        if (!dispatch_one_(
                question_queue,
                database,
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
        struct B_Database *database,
        B_QuestionDispatchCallback *callback,
        void *callback_opaque,
        bool *keep_going,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(question_queue);
    B_ASSERT(database);
    B_ASSERT(callback);
    B_ASSERT(keep_going);

    struct QuestionDispatchClosure_ *closure = NULL;
    struct B_QuestionQueueItemObject *queue_item = NULL;

    if (!b_question_queue_dequeue(
            question_queue, &queue_item, eh)) {
        goto fail;
    }
    if (!queue_item) {
        *keep_going = false;
        return true;
    }
    B_ASSERT(queue_item->question);
    B_ASSERT(queue_item->question_vtable);
    B_ASSERT(queue_item->answer_callback);

    // Try asking the database for an answer.
    struct B_Answer *answer;
    if (b_database_look_up_answer(
            database,
            queue_item->question,
            queue_item->question_vtable,
            &answer,
            eh) && answer) {
        bool ok = queue_item->answer_callback(
            answer, queue_item, eh);
        (void) b_question_queue_item_object_deallocate(
            queue_item, eh);
        return ok;
    }

    // Answer the question The Hard Way.
    // FIXME(strager): If QuestionDispatchClosure_ and
    // B_AnswerContext have the same lifetime, they can be
    // merged into one allocation.
    if (!b_allocate(
            sizeof(*closure), (void **) &closure, eh)) {
        goto fail;
    }
    *closure = (struct QuestionDispatchClosure_) {
        .answer_context = {
            .question = queue_item->question,
            .question_vtable = queue_item->question_vtable,
            .answer_callback = answer_callback_,
            .answer_callback_opaque = closure,
            .question_queue = question_queue,
            .database = database,
        },
        .queue_item = queue_item,
        .database = database,
    };

    if (!callback(
            &closure->answer_context,
            callback_opaque,
            eh)) {
        goto fail;
    }

    *keep_going = true;
    return true;

fail:
    if (queue_item) {
        // FIXME(strager): Should we re-enqueue instead?
        (void) b_question_queue_item_object_deallocate(
            queue_item, eh);
    }
    if (closure) {
        (void) b_deallocate(closure, eh);
    }
    return false;
}

static B_FUNC
answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *answer,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    struct QuestionDispatchClosure_ *closure = opaque;
    B_CHECK_PRECONDITION(eh, closure);
    B_CHECK_PRECONDITION(eh, closure->queue_item);
    B_CHECK_PRECONDITION(eh, closure->queue_item->answer_callback);

    if (!b_database_record_answer(
            closure->database,
            closure->answer_context.question,
            closure->answer_context.question_vtable,
            answer,
            eh)) {
        return false;
    }

    if (!closure->queue_item->answer_callback(
            answer, closure->queue_item, eh)) {
        return false;
    }

    (void) b_question_queue_item_object_deallocate(
        closure->queue_item, eh);
    (void) b_deallocate(closure, eh);
    return true;
}
