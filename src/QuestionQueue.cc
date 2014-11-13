#include <B/Assert.h>
#include <B/Alloc.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/QuestionQueue.h>

#if defined(B_CONFIG_PTHREAD)
# include <B/Private/Thread.h>
#endif

#include <deque>
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

struct B_QuestionQueue {
    B_QuestionQueue() :
            closed(false) {
        bool ok = b_mutex_initialize(&this->lock, nullptr);
        B_ASSERT(ok);
    }

    ~B_QuestionQueue() {
        static_cast<void>(b_mutex_destroy(
            &this->lock, nullptr));
        B_ASSERT(this->queue_items.empty());
    }

    void
    deallocate(B_ErrorHandler const *eh) {
        for (auto queue_item : this->queue_items) {
            if (!b_question_queue_item_object_deallocate(
                    queue_item, eh)) {
                // Continue deallocation.
            }
        }
        this->queue_items.clear();
    }

    void
    enqueue(B_QuestionQueueItem *queue_item) {
        B_ASSERT(queue_item);

        {
            B_MutexHolder locker(&this->lock);

            queue_items.push_back(queue_item);
            this->signal_enqueued();
        }
    }

    B_QuestionQueueItem *
    try_dequeue(bool *closed) {
        {
            B_MutexHolder locker(&this->lock);

            if (this->closed) {
                *closed = true;
                return nullptr;
            }
            *closed = false;

            if (queue_items.empty()) {
                return nullptr;
            } else {
                auto front = this->queue_items.front();
                this->queue_items.pop_front();
                return front;
            }
        }
    }

    void
    close() {
        {
            B_MutexHolder locker(&this->lock);

            this->closed = true;
            this->signal_enqueued();
        }
    }

protected:
    // ::lock is held.
    virtual void
    signal_enqueued() = 0;

    B_Mutex lock;

private:
    bool closed;
    // TODO(strager): Use the B allocator or stop using STL.
    std::deque<B_QuestionQueueItem *> queue_items;
};

namespace {

class QuestionQueueSingleThreaded : public B_QuestionQueue {
public:
    void
    signal_enqueued() override {
        // Do nothing.
    }
};

#if defined(B_CONFIG_PTHREAD)
class QuestionQueuePthreadCond : public B_QuestionQueue {
public:
    QuestionQueuePthreadCond(
            B_BORROWED pthread_cond_t *cond) :
            cond(cond) {
    }

    void
    signal_enqueued() override {
        int rc = pthread_cond_signal(this->cond);
        B_ASSERT(rc == 0);
    }

private:
    pthread_cond_t *cond;
};
#endif

#if defined(B_CONFIG_KQUEUE)
class QuestionQueueKqueue : public B_QuestionQueue {
public:
    QuestionQueueKqueue(
            B_BORROWED int kqueue,
            B_BORROWED const struct kevent *kevent) :
            kqueue(kqueue),
            kevent(*kevent) {
    }

    void
    signal_enqueued() override {
        int rc = ::kevent(
            this->kqueue,
            &this->kevent,
            1,
            nullptr,
            0,
            nullptr);
        B_ASSERT(rc == 0);  // FIXME(strager)
    }

private:
    int kqueue;
    struct kevent kevent;
};
#endif

#if defined(B_CONFIG_EVENTFD)
class QuestionQueueEventfd : public B_QuestionQueue {
public:
    QuestionQueueEventfd(
            B_BORROWED int eventfd) :
            eventfd(eventfd) {
    }

    void
    signal_enqueued() override {
        B_ErrorHandler *eh = nullptr;  // FIXME(strager)
        bool ok = b_eventfd_write(this->eventfd, 1, eh);
        (void) ok;  // FIXME(strager)
    }

private:
    int eventfd;
};
#endif

}

B_EXPORT_FUNC
b_question_queue_allocate_single_threaded(
        B_OUTPTR B_QuestionQueue **out,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    QuestionQueueSingleThreaded *queue;
    if (!b_new(&queue, eh)) {
        return false;
    }
    *out = queue;
    return true;
}

B_EXPORT_FUNC
b_question_queue_allocate_with_pthread_cond(
        void *cond,
        B_OUTPTR B_QuestionQueue **out,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, cond);
    B_CHECK_PRECONDITION(eh, out);

#if defined(B_CONFIG_PTHREAD)
    QuestionQueuePthreadCond *queue;
    if (!b_new(
            &queue,
            eh,
            static_cast<pthread_cond_t *>(cond))) {
        return false;
    }
    *out = queue;
    return true;
#else
    (void) B_RAISE_ERRNO_ERROR(
        eh,
        ENOTSUP,
        "b_question_queue_allocate_with_pthread_cond");
    return false;
#endif
}

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
    QuestionQueueKqueue *queue;
    if (!b_new(
            &queue,
            eh,
            kqueue_fd,
            static_cast<struct kevent const *>(trigger))) {
        return false;
    }
    *out = queue;
    return true;
#else
    (void) B_RAISE_ERRNO_ERROR(
        eh,
        ENOTSUP,
        "b_question_queue_allocate_with_kqueue");
    return false;
#endif
}

B_EXPORT_FUNC
b_question_queue_allocate_with_eventfd(
        B_BORROWED int eventfd,
        B_OUTPTR struct B_QuestionQueue **out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, eventfd >= 0);
    B_CHECK_PRECONDITION(eh, out);

#if defined(B_CONFIG_EVENTFD)
    QuestionQueueEventfd *queue;
    if (!b_new(&queue, eh, eventfd)) {
        return false;
    }
    *out = queue;
    return true;
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
        B_QuestionQueue *queue,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);

    queue->deallocate(eh);
    return b_delete(queue, eh);
}

B_EXPORT_FUNC
b_question_queue_enqueue(
        B_QuestionQueue *queue,
        B_TRANSFER B_QuestionQueueItem *queue_item,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);
    B_CHECK_PRECONDITION(eh, queue_item);

    B_CHECK_PRECONDITION(eh, queue_item->deallocate);
    B_CHECK_PRECONDITION(eh, queue_item->question);
    B_CHECK_PRECONDITION(eh, queue_item->question_vtable);
    B_CHECK_PRECONDITION(eh, queue_item->answer_callback);

    queue->enqueue(queue_item);
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

    *out = queue->try_dequeue(closed);
    return true;
}

B_EXPORT_FUNC
b_question_queue_close(
        struct B_QuestionQueue *queue,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);

    queue->close();
    return true;
}

B_EXPORT_FUNC
b_question_queue_item_object_deallocate(
        struct B_QuestionQueueItem *queue_item,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue_item);
    B_CHECK_PRECONDITION(eh, queue_item->deallocate);

    return queue_item->deallocate(queue_item, eh);
}
