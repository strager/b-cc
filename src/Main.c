#include <B/Assert.h>
#include <B/Database.h>
#include <B/Error.h>
#include <B/Macro.h>
#include <B/Main.h>
#include <B/Process.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>

#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>

struct RootQueueItem_ {
    struct B_QuestionQueueItem super;

    struct B_QuestionQueue *queue;

    // Assigned when answer_callback is called.
    bool did_answer;
    struct B_Answer *answer;

    // Assigned when dealloate is called.
    bool did_deallocate;
};

B_STATIC_ASSERT(
    offsetof(struct RootQueueItem_, super) == 0,
    "RootQueueItem_::super must be the first member");

static B_FUNC
root_queue_item_deallocate_(
        struct B_QuestionQueueItem *super,
        struct B_ErrorHandler const *eh) {
    struct RootQueueItem_ *root_queue_item
        = (struct RootQueueItem_ *) super;
    B_CHECK_PRECONDITION(eh, root_queue_item);

    B_ASSERT(!root_queue_item->did_deallocate);
    root_queue_item->did_deallocate = true;

    return true;
}

static B_FUNC
root_queue_item_answer_(
        B_TRANSFER B_OPT struct B_Answer *answer,
        void *super,
        struct B_ErrorHandler const *eh) {
    struct RootQueueItem_ *root_queue_item = super;
    B_CHECK_PRECONDITION(eh, root_queue_item);

    B_ASSERT(!root_queue_item->did_answer);
    root_queue_item->did_answer = true;
    root_queue_item->answer = answer;

    if (!b_question_queue_close(
            root_queue_item->queue, eh)) {
        return false;
    }

    return true;
}

B_EXPORT_FUNC
b_main(
        struct B_Question const *initial_question,
        struct B_QuestionVTable const *initial_question_vtable,
        B_OUTPTR struct B_Answer **answer,
        char const *database_sqlite_path,
        struct B_QuestionVTableSet const *vtable_set,
        B_QuestionDispatchCallback dispatch_callback,
        void *opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, initial_question);
    B_CHECK_PRECONDITION(eh, initial_question_vtable);
    B_CHECK_PRECONDITION(eh, answer);
    B_CHECK_PRECONDITION(eh, database_sqlite_path);
    B_CHECK_PRECONDITION(eh, vtable_set);
    B_CHECK_PRECONDITION(eh, dispatch_callback);

    bool ok;
    struct B_ProcessLoop *process_loop = NULL;
    struct B_QuestionQueue *question_queue = NULL;
    struct B_Database *database = NULL;
    struct RootQueueItem_ root_queue_item = {
        .super = {
            .deallocate = root_queue_item_deallocate_,
            .question = (struct B_Question *)
                initial_question,
            .question_vtable = initial_question_vtable,
            .answer_callback = root_queue_item_answer_,
        },
        .queue = NULL,
        .did_answer = false,
        .answer = NULL,
        .did_deallocate = false,
    };

    if (!b_process_loop_allocate(
            1,
            b_process_auto_configuration_unsafe(),
            &process_loop,
            eh)) {
        goto fail;
    }
    if (!b_process_loop_run_async_unsafe(
            process_loop, eh)) {
        goto fail;
    }

    if (!b_database_load_sqlite(
            database_sqlite_path,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL,
            &database,
            eh)) {
        goto fail;
    }
    if (!b_database_recheck_all(database, vtable_set, eh)) {
        goto fail;
    }

    if (!b_question_queue_allocate(
            &question_queue, eh)) {
        goto fail;
    }
    root_queue_item.queue = question_queue;

    if (!b_question_queue_enqueue(
            question_queue, &root_queue_item.super, eh)) {
        goto fail;
    }

    // Drain the queue.
    struct B_MainClosure closure = {
        .process_loop = process_loop,
        .opaque = opaque,
    };
    while (true) {
        struct B_QuestionQueueItem *queue_item = NULL;
        if (!b_question_queue_dequeue(
                question_queue, &queue_item, eh)) {
            goto fail;
        }
        if (!queue_item) {
            break;
        }

        if (!b_question_dispatch_one(
                queue_item,
                question_queue,
                database,
                dispatch_callback,
                &closure,
                eh)) {
            goto fail;
        }
    }

    if (!root_queue_item.did_answer) {
        goto fail;
    }

    B_ASSERT(root_queue_item.did_deallocate);
    *answer = root_queue_item.answer;

    ok = true;

done:
    if (!ok && root_queue_item.answer) {
        (void) initial_question_vtable->answer_vtable
            ->deallocate(root_queue_item.answer, eh);
    }
    if (database) {
        (void) b_database_close(database, eh);
    }
    if (question_queue) {
        (void) b_question_queue_deallocate(
            question_queue, eh);
    }
    if (process_loop) {
        uint64_t process_force_kill_timeout_picoseconds = 0;
        (void) b_process_loop_deallocate(
            process_loop,
            process_force_kill_timeout_picoseconds,
            eh);
    }
    return ok;

fail:
    ok = false;
    goto done;
}
