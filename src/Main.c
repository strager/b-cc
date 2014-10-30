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

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

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
    struct B_QuestionQueueRoot *question_queue_root = NULL;
    struct B_Answer *tmp_answer = NULL;

#if defined(B_CONFIG_PTHREAD)
    pthread_cond_t question_queue_cond;
    bool did_init_question_queue_cond = false;
#endif

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

#if defined(B_CONFIG_PTHREAD)
    int rc = pthread_cond_init(&question_queue_cond, NULL);
    B_ASSERT(rc == 0);
    did_init_question_queue_cond = true;
#else
# error "Unknown question queue implementation"
#endif
    if (!b_question_queue_allocate_with_pthread_cond(
            &question_queue_cond, &question_queue, eh)) {
        goto fail;
    }
    if (!b_question_queue_enqueue_root(
            question_queue,
            initial_question,
            initial_question_vtable,
            &question_queue_root,
            eh)) {
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

    if (!b_question_queue_finalize_root(
            question_queue_root, &tmp_answer, eh)) {
        goto fail;
    }
    question_queue_root = NULL;  // Transferred.
    if (!tmp_answer) {
        (void) eh;  // TODO(strager)
        goto fail;
    }
    *answer = tmp_answer;

    ok = true;

done:
    if (!ok && tmp_answer) {
        (void) initial_question_vtable->answer_vtable
            ->deallocate(tmp_answer, eh);
    }
    if (database) {
        (void) b_database_close(database, eh);
    }
    if (question_queue) {
        (void) b_question_queue_deallocate(
            question_queue, eh);
    }
#if defined(B_CONFIG_PTHREAD)
    if (did_init_question_queue_cond) {
        rc = pthread_cond_destroy(&question_queue_cond);
        B_ASSERT(rc == 0);
    }
#endif
    if (process_loop) {
        uint64_t process_force_kill_timeout_picoseconds = 0;
        (void) b_process_loop_deallocate(
            process_loop,
            process_force_kill_timeout_picoseconds,
            eh);
    }
    if (question_queue_root) {
        (void) b_question_queue_finalize_root(
            question_queue_root, &tmp_answer, eh);
    }
    return ok;

fail:
    ok = false;
    goto done;
}
