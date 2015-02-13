#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/Private/Queue.h>
#include <B/QuestionQueue.h>

#if defined(B_CONFIG_PTHREAD)
# include <B/Private/Thread.h>
#endif

#include <errno.h>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

#if defined(B_CONFIG_KQUEUE)
# include <sys/event.h>
#endif

#if defined(B_CONFIG_EVENTFD)
# include <B/Private/OS.h>
#endif

struct QuestionQueueEntry_ {
    B_SIMPLEQ_ENTRY(QuestionQueueEntry_) link;
    struct B_QuestionQueueItem *queue_item;
};

struct B_QuestionQueue {
    B_FUNC (*B_CONST_STRUCT_MEMBER signal_enqueued_locked)(
        struct B_QuestionQueue *,
        struct B_ErrorHandler const *);
    B_FUNC (*B_CONST_STRUCT_MEMBER deallocate)(
        B_TRANSFER struct B_QuestionQueue *,
        struct B_ErrorHandler const *);
    struct B_Mutex lock;

    // Locked by lock.
    B_SIMPLEQ_HEAD(, QuestionQueueEntry_) entries;
    bool closed;
};

static B_FUNC
question_queue_force_try_dequeue_locked_(
        struct B_QuestionQueue *,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *,
        struct B_ErrorHandler const *);

static B_FUNC
question_queue_deallocate_base_(
        B_TRANSFER struct B_QuestionQueue *,
        struct B_ErrorHandler const *);

static B_FUNC
question_queue_initialize_base_(
        struct B_QuestionQueue *queue,
        B_FUNC (*signal_enqueued_locked)(
            struct B_QuestionQueue *,
            struct B_ErrorHandler const *),
        B_FUNC (*B_OPT deallocate)(
            struct B_QuestionQueue *,
            struct B_ErrorHandler const *),
        struct B_ErrorHandler const *eh) {
    B_ASSERT(signal_enqueued_locked);

    *queue = (struct B_QuestionQueue) {
        .signal_enqueued_locked = signal_enqueued_locked,
        .deallocate = deallocate
            ? deallocate
            : question_queue_deallocate_base_,
        // .lock
        // .entries
        .closed = false,
    };
    if (!b_mutex_initialize(&queue->lock, eh)) {
        return false;
    }
    B_SIMPLEQ_INIT(&queue->entries);
    return true;
}

static B_FUNC
question_queue_destroy_base_(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    bool ok = true;

    for (;;) {
        struct B_QuestionQueueItem *item;
        if (!question_queue_force_try_dequeue_locked_(
                queue, &item, eh)) {
            ok = false;
            // Dequeueing again probably won't do anything.
            break;
        }
        if (!item) {
            break;
        }
        if (!b_question_queue_item_object_deallocate(
                item, eh)) {
            ok = false;
            // Continue dequeue loop.
        }
    }
    B_ASSERT(B_SIMPLEQ_EMPTY(&queue->entries));

    if (!b_mutex_destroy(&queue->lock, eh)) {
        ok = false;
    }

    return ok;
}

static B_FUNC
question_queue_deallocate_base_(
        B_TRANSFER struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    (void) question_queue_destroy_base_(queue, eh);
    (void) b_deallocate(queue, eh);
    return true;
}

static B_FUNC
single_threaded_signal_enqueued_locked_(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    // Do nothing.
    (void) queue;
    (void) eh;
    return true;
}

static B_FUNC
question_queue_force_try_dequeue_locked_(
        struct B_QuestionQueue *queue,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *out,
        struct B_ErrorHandler const *eh) {
    if (B_SIMPLEQ_EMPTY(&queue->entries)) {
        *out = NULL;
        return true;
    } else {
        struct QuestionQueueEntry_ *entry
            = B_SIMPLEQ_FIRST(&queue->entries);
        B_SIMPLEQ_REMOVE_HEAD(&queue->entries, link);
        *out = entry->queue_item;
        (void) b_deallocate(entry, eh);
        return true;
    }
}

B_EXPORT_FUNC
b_question_queue_allocate_single_threaded(
        B_OUTPTR struct B_QuestionQueue **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    struct B_QuestionQueue *queue = NULL;
    bool initialized = true;

    if (!b_allocate(sizeof(*queue), (void **) &queue, eh)) {
        goto fail;
    }
    if (!question_queue_initialize_base_(
            queue,
            single_threaded_signal_enqueued_locked_,
            NULL,
            eh)) {
        goto fail;
    }
    initialized = true;
    *out = queue;
    return true;

fail:
    if (initialized) {
        (void) question_queue_destroy_base_(queue, eh);
    }
    if (queue) {
        (void) b_deallocate(queue, eh);
    }
    return false;
}

#if defined(B_CONFIG_PTHREAD)
struct QuestionQueuePthreadCond_ {
    struct B_QuestionQueue super;
    pthread_cond_t *B_CONST_STRUCT_MEMBER cond;
};

B_STATIC_ASSERT(
    offsetof(struct QuestionQueuePthreadCond_, super) == 0,
    "QuestionQueuePthreadCond_::super must be the first "
        "member");

static B_FUNC
pthread_cond_signal_enqueued_locked_(
        struct B_QuestionQueue *queue_raw,
        struct B_ErrorHandler const *eh) {
    struct QuestionQueuePthreadCond_ *queue
        = (void *) queue_raw;
    int rc = pthread_cond_signal(queue->cond);
    if (rc != 0) {
        // TODO(strager)
        (void) B_RAISE_ERRNO_ERROR(
            eh, rc, "pthread_cond_signal");
    }
    return true;
}
#endif

B_EXPORT_FUNC
b_question_queue_allocate_with_pthread_cond(
        void *cond,
        B_OUTPTR struct B_QuestionQueue **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, cond);
    B_CHECK_PRECONDITION(eh, out);

#if defined(B_CONFIG_PTHREAD)
    struct QuestionQueuePthreadCond_ *queue = NULL;
    bool initialized = true;

    if (!b_allocate(sizeof(*queue), (void **) &queue, eh)) {
        goto fail;
    }
    *queue = (struct QuestionQueuePthreadCond_) {
        // .base
        .cond = cond,
    };
    if (!question_queue_initialize_base_(
            &queue->super,
            pthread_cond_signal_enqueued_locked_,
            NULL,
            eh)) {
        goto fail;
    }
    initialized = true;
    *out = &queue->super;
    return true;

fail:
    if (initialized) {
        (void) question_queue_destroy_base_(
            &queue->super, eh);
    }
    if (queue) {
        (void) b_deallocate(queue, eh);
    }
    return false;
#else
    (void) B_RAISE_ERRNO_ERROR(
        eh,
        ENOTSUP,
        "b_question_queue_allocate_with_pthread_cond");
    return false;
#endif
}

#if defined(B_CONFIG_KQUEUE)
struct QuestionQueueKqueue_ {
    struct B_QuestionQueue super;
    int B_CONST_STRUCT_MEMBER kqueue;
    struct kevent B_CONST_STRUCT_MEMBER kevent;
};

B_STATIC_ASSERT(
    offsetof(struct QuestionQueueKqueue_, super) == 0,
    "QuestionQueueKqueue_::super must be the first "
        "member");

static B_FUNC
kqueue_signal_enqueued_locked_(
        struct B_QuestionQueue *queue_raw,
        struct B_ErrorHandler const *eh) {
    struct QuestionQueueKqueue_ *queue = (void *) queue_raw;
    int rc = kevent(
        queue->kqueue,
        &queue->kevent,
        1,
        NULL,
        0,
        NULL);
    if (rc != 0) {
        // TODO(strager)
        (void) B_RAISE_ERRNO_ERROR(
            eh, errno, "kevent");
    }
    return true;
}
#endif

B_EXPORT_FUNC
b_question_queue_allocate_with_kqueue(
        B_BORROWED int kqueue_fd,
        B_BORROWED const void *trigger,  // kevent
        B_OUTPTR struct B_QuestionQueue **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, kqueue_fd >= 0);
    B_CHECK_PRECONDITION(eh, trigger);
    B_CHECK_PRECONDITION(eh, out);

#if defined(B_CONFIG_KQUEUE)
    struct QuestionQueueKqueue_ *queue = NULL;
    bool initialized = true;

    if (!b_allocate(sizeof(*queue), (void **) &queue, eh)) {
        goto fail;
    }
    *queue = (struct QuestionQueueKqueue_) {
        // .base
        .kqueue = kqueue_fd,
        .kevent = *(struct kevent *) trigger,
    };
    if (!question_queue_initialize_base_(
            &queue->super,
            kqueue_signal_enqueued_locked_,
            NULL,
            eh)) {
        goto fail;
    }
    initialized = true;
    *out = &queue->super;
    return true;

fail:
    if (initialized) {
        (void) question_queue_destroy_base_(
            &queue->super, eh);
    }
    if (queue) {
        (void) b_deallocate(queue, eh);
    }
    return false;
#else
    (void) B_RAISE_ERRNO_ERROR(
        eh,
        ENOTSUP,
        "b_question_queue_allocate_with_kqueue");
    return false;
#endif
}

#if defined(B_CONFIG_EVENTFD)
struct QuestionQueueEventfd_ {
    struct B_QuestionQueue super;
    int B_CONST_STRUCT_MEMBER eventfd;
};

B_STATIC_ASSERT(
    offsetof(struct QuestionQueueEventfd_, super) == 0,
    "QuestionQueueEventfd_::super must be the first "
        "member");

static B_FUNC
eventfd_signal_enqueued_locked_(
        struct B_QuestionQueue *queue_raw,
        struct B_ErrorHandler const *eh) {
    struct QuestionQueueEventfd_ *queue = (void *) queue_raw;
    return b_eventfd_write(queue->eventfd, 1, eh);
}

static B_FUNC
eventfd_deallocate_(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    (void) question_queue_destroy_base_(queue, eh);
    (void) b_deallocate(queue, eh);
    return true;
}
#endif

B_EXPORT_FUNC
b_question_queue_allocate_with_eventfd(
        B_BORROWED int eventfd,
        B_OUTPTR struct B_QuestionQueue **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, eventfd >= 0);
    B_CHECK_PRECONDITION(eh, out);

#if defined(B_CONFIG_EVENTFD)
    struct QuestionQueueEventfd_ *queue = NULL;
    bool initialized = true;

    if (!b_allocate(sizeof(*queue), (void **) &queue, eh)) {
        goto fail;
    }
    *queue = (struct QuestionQueueEventfd_) {
        // .base
        .eventfd = eventfd,
    };
    if (!question_queue_initialize_base_(
            &queue->super,
            eventfd_signal_enqueued_locked_,
            eventfd_deallocate_,
            eh)) {
        goto fail;
    }
    initialized = true;
    *out = &queue->super;
    return true;

fail:
    if (initialized) {
        (void) question_queue_destroy_base_(
            &queue->super, eh);
    }
    if (queue) {
        (void) b_deallocate(queue, eh);
    }
    return false;
#else
    (void) B_RAISE_ERRNO_ERROR(
        eh,
        ENOTSUP,
        "b_question_queue_allocate_with_eventfd");
    return false;
#endif
}

B_EXPORT_FUNC
b_question_queue_deallocate(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);
    return queue->deallocate(queue, eh);
}

static B_FUNC
question_queue_enqueue_locked_(
        struct B_QuestionQueue *queue,
        B_TRANSFER struct B_QuestionQueueItem *queue_item,
        struct B_ErrorHandler const *eh) {
    // FIXME(strager): Should we assert the queue is not
    // closed?

    struct QuestionQueueEntry_ *entry;
    if (!b_allocate(sizeof(*entry), (void **) &entry, eh)) {
        return false;
    }
    *entry = (struct QuestionQueueEntry_) {
        // .link
        .queue_item = queue_item,
    };
    B_SIMPLEQ_INSERT_TAIL(&queue->entries, entry, link);

    (void) queue->signal_enqueued_locked(queue, eh);
    return true;
}

B_EXPORT_FUNC
b_question_queue_enqueue(
        struct B_QuestionQueue *queue,
        B_TRANSFER struct B_QuestionQueueItem *queue_item,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);
    B_CHECK_PRECONDITION(eh, queue_item);

    B_CHECK_PRECONDITION(eh, queue_item->deallocate);
    B_CHECK_PRECONDITION(eh, queue_item->question);
    B_CHECK_PRECONDITION(eh, queue_item->question_vtable);
    B_CHECK_PRECONDITION(eh, queue_item->answer_callback);

    b_mutex_lock(&queue->lock);
    bool ok = question_queue_enqueue_locked_(
        queue, queue_item, eh);
    b_mutex_unlock(&queue->lock);
    return ok;
}

static B_FUNC
question_queue_try_dequeue_locked_(
        struct B_QuestionQueue *queue,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *out,
        B_OUT bool *closed,
        struct B_ErrorHandler const *eh) {
    if (queue->closed) {
        *closed = true;
        return true;
    }
    if (!question_queue_force_try_dequeue_locked_(
            queue, out, eh)) {
        return false;
    }
    *closed = false;
    return true;
}

B_EXPORT_FUNC
b_question_queue_try_dequeue(
        struct B_QuestionQueue *queue,
        B_OUTPTR struct B_QuestionQueueItem *B_OPT *out,
        B_OUT bool *closed,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);
    B_CHECK_PRECONDITION(eh, out);

    b_mutex_lock(&queue->lock);
    bool ok = question_queue_try_dequeue_locked_(
        queue, out, closed, eh);
    b_mutex_unlock(&queue->lock);
    return ok;
}

static B_FUNC
question_queue_close_locked_(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    if (queue->closed) {
        return true;
    }
    queue->closed = true;
    (void) queue->signal_enqueued_locked(queue, eh);
    return true;
}

B_EXPORT_FUNC
b_question_queue_close(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);

    b_mutex_lock(&queue->lock);
    bool ok = question_queue_close_locked_(queue, eh);
    b_mutex_unlock(&queue->lock);
    return ok;
}

B_EXPORT_FUNC
b_question_queue_item_object_deallocate(
        struct B_QuestionQueueItem *queue_item,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue_item);
    B_CHECK_PRECONDITION(eh, queue_item->deallocate);

    return queue_item->deallocate(queue_item, eh);
}
