#include <B/Assert.h>
#include <B/Alloc.h>
#include <B/Config.h>
#include <B/Error.h>
#include <B/QuestionQueue.h>

#if defined(B_CONFIG_PTHREAD)
# include <B/Thread.h>
#endif

#include <deque>

#if defined(B_CONFIG_PTHREAD)
# include <pthread.h>
#endif

struct B_QuestionQueue {
    B_QuestionQueue() :
#if defined(B_CONFIG_PTHREAD)
            lock(PTHREAD_MUTEX_INITIALIZER),
            cond(PTHREAD_COND_INITIALIZER),
#endif
            closed(false),
            queue_items(B_Allocator<std::deque<
                    B_QuestionQueueItemObject *>>()) {
    }

    ~B_QuestionQueue() {
        B_ASSERT(this->queue_items.empty());
    }

    void
    deallocate(
            B_ErrorHandler const *eh) {
        for (auto queue_item : this->queue_items) {
            if (!b_question_queue_item_object_deallocate(
                    queue_item,
                    eh)) {
                // Continue deallocation.
            }
        }
    }

#if defined(B_CONFIG_PTHREAD)
    void
    enqueue(
            B_QuestionQueueItemObject *queue_item) {
        B_ASSERT(queue_item);

        {
            B_PthreadMutexHolder locker(&this->lock);

            queue_items.push_back(queue_item);
            int rc = pthread_cond_signal(&this->cond);
            B_ASSERT(rc == 0);
        }
    }

    B_QuestionQueueItemObject *
    dequeue() {
        {
            B_PthreadMutexHolder locker(&this->lock);

            while (queue_items.empty() && !this->closed) {
                int rc = pthread_cond_wait(
                        &this->cond,
                        &this->lock);
                B_ASSERT(rc == 0);
            }
            if (this->closed) {
                return nullptr;
            } else {
                B_ASSERT(!queue_items.empty());
                auto front = this->queue_items.front();
                this->queue_items.pop_front();
                return front;
            }
        }
    }

    void
    close() {
        {
            B_PthreadMutexHolder locker(&this->lock);

            this->closed = true;
            int rc = pthread_cond_signal(&this->cond);
            B_ASSERT(rc == 0);
        }
    }
#else
# error "Unknown threads implementation"
#endif

private:
#if defined(B_CONFIG_PTHREAD)
    pthread_mutex_t lock;
    pthread_cond_t cond;
#endif

    bool closed;
    std::deque<
            B_QuestionQueueItemObject *,
            B_Allocator<B_QuestionQueueItemObject *>> queue_items;
};

B_EXPORT_FUNC
b_question_queue_allocate(
        B_OUTPTR B_QuestionQueue **out,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, out);

    return b_new(out, eh);
}

B_EXPORT_FUNC
b_question_queue_deallocate(
        B_QuestionQueue *queue,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);

    queue->deallocate(eh);
    delete queue;
    return true;
}

B_EXPORT_FUNC
b_question_queue_enqueue(
        B_QuestionQueue *queue,
        B_TRANSFER B_QuestionQueueItemObject *queue_item,
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
b_question_queue_dequeue(
        B_QuestionQueue *queue,
        B_OUTPTR B_QuestionQueueItemObject **out,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue);
    B_CHECK_PRECONDITION(eh, out);

    *out = queue->dequeue();
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
        struct B_QuestionQueueItemObject *queue_item,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, queue_item);
    B_CHECK_PRECONDITION(eh, queue_item->deallocate);

    return queue_item->deallocate(queue_item, eh);
}
