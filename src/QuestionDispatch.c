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
    struct B_QuestionQueueItem *B_CONST_STRUCT_MEMBER queue_item;
    struct B_Database *B_CONST_STRUCT_MEMBER database;
};

static B_FUNC
answer_callback_(
        B_TRANSFER B_OPT struct B_Answer *,
        void *opaque,
        struct B_ErrorHandler const *);

B_EXPORT_FUNC
b_question_dispatch_one(
        B_TRANSFER struct B_QuestionQueueItem *queue_item,
        struct B_QuestionQueue *question_queue,
        struct B_Database *database,
        B_OUTPTR B_OPT struct B_AnswerContext const **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue_item);
    B_CHECK_PRECONDITION(eh, queue_item->question);
    B_CHECK_PRECONDITION(eh, queue_item->question_vtable);
    B_CHECK_PRECONDITION(eh, queue_item->answer_callback);
    B_CHECK_PRECONDITION(eh, question_queue);
    B_CHECK_PRECONDITION(eh, database);
    B_CHECK_PRECONDITION(eh, out);

    struct QuestionDispatchClosure_ *closure = NULL;

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
        *out = NULL;
        return ok;
    }

    // Answer the question The Hard Way.
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

    *out = &closure->answer_context;
    return true;

fail:
    // FIXME(strager): Should we (or the caller) re-enqueue
    // instead?
    (void) b_question_queue_item_object_deallocate(
        queue_item, eh);
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

    if (answer) {
        if (!b_database_record_answer(
                closure->database,
                closure->answer_context.question,
                closure->answer_context.question_vtable,
                answer,
                eh)) {
            return false;
        }
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
