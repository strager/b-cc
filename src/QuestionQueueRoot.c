#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Error.h>
#include <B/QuestionQueue.h>

struct B_QuestionQueueRoot {
    struct B_QuestionQueueItem super;

    struct B_QuestionQueue *queue;

    // Assigned when answer_callback is called.
    bool did_answer;
    struct B_Answer *answer;

    // Assigned when dealloate is called.
    bool did_deallocate;
};

static B_FUNC
root_deallocate_(
        struct B_QuestionQueueItem *,
        struct B_ErrorHandler const *);

static B_FUNC
root_answer_(
        B_TRANSFER B_OPT struct B_Answer *,
        void *,
        struct B_ErrorHandler const *);

B_STATIC_ASSERT(
    offsetof(struct B_QuestionQueueRoot, super) == 0,
    "B_QuestionQueueRoot::super must be the first member");

B_EXPORT_FUNC
b_question_queue_enqueue_root(
        B_BORROWED struct B_QuestionQueue *queue,
        B_BORROWED const struct B_Question *question,
        B_BORROWED const struct B_QuestionVTable *
            question_vtable,
        B_OUTPTR struct B_QuestionQueueRoot **out_root,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);
    B_CHECK_PRECONDITION(eh, question);
    B_CHECK_PRECONDITION(eh, question_vtable);
    B_CHECK_PRECONDITION(eh, out_root);

    struct B_QuestionQueueRoot *root = NULL;

    if (!b_allocate(sizeof(*root), (void **) &root, eh)) {
        goto fail;
    }
    *root = (struct B_QuestionQueueRoot) {
        .super = {
            .deallocate = root_deallocate_,
            .question = (struct B_Question *) question,
            .question_vtable = question_vtable,
            .answer_callback = root_answer_,
        },
        .queue = queue,
        .did_answer = false,
        .answer = NULL,
        .did_deallocate = false,
    };

    if (!b_question_queue_enqueue(
            queue, &root->super, eh)) {
        goto fail;
    }

    *out_root = root;
    return true;

fail:
    if (root) {
        (void) b_deallocate(root, eh);
    }
    return false;
}

// Deallocates a QuestionQueueRoot and returns the Answer to
// the Question given to b_question_queue_enqueue_root.
B_EXPORT_FUNC
b_question_queue_finalize_root(
        B_TRANSFER struct B_QuestionQueueRoot *root,
        B_OUTPTR B_TRANSFER struct B_Answer **out_answer,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, root);
    B_CHECK_PRECONDITION(eh, out_answer);

    // FIXME(strager): Should this error on failure instead?
    B_ASSERT(root->did_deallocate);

    if (root->did_answer) {
        *out_answer = root->answer;
    } else {
        *out_answer = NULL;
    }

    return b_deallocate(root, eh);
}

static B_FUNC
root_deallocate_(
        struct B_QuestionQueueItem *super,
        struct B_ErrorHandler const *eh) {
    struct B_QuestionQueueRoot *root_queue_item
        = (struct B_QuestionQueueRoot *) super;
    B_CHECK_PRECONDITION(eh, root_queue_item);

    B_ASSERT(!root_queue_item->did_deallocate);
    root_queue_item->did_deallocate = true;

    return true;
}

static B_FUNC
root_answer_(
        B_TRANSFER B_OPT struct B_Answer *answer,
        void *super,
        struct B_ErrorHandler const *eh) {
    struct B_QuestionQueueRoot *root_queue_item = super;
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
