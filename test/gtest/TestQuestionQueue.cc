#include "Mocks.h"

#include <B/Config.h>
#include <B/QuestionQueue.h>

#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#if defined(B_CONFIG_POSIX)
# include <pthread.h>
#endif

#if defined(B_CONFIG_KQUEUE)
# include <sys/event.h>
#endif

#if defined(B_CONFIG_EVENTFD)
# include <fcntl.h>
# include <poll.h>
# include <sys/eventfd.h>
#endif

using namespace testing;

namespace Testers {

class Tester {
public:
    class Factory {
    public:
        virtual std::unique_ptr<Tester>
        create_tester() const = 0;

        virtual const char *
        get_type_string() const = 0;
    };

    virtual ~Tester() {
    }

    virtual B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **,
            B_ErrorHandler const *) = 0;

    // Returns true if try_consume_event can have false
    // positives.
    virtual bool
    consumes_spurious_events() = 0;

    // Tests if an event was given by the QuestionQueue.  If
    // so, the event is consumed.
    //
    // May have false positives iff consumes_spurious_events
    // returns true.
    virtual bool
    try_consume_event() = 0;
};

class SingleThreadedTester : public Tester {
public:
    class Factory : public Tester::Factory {
    public:
        std::unique_ptr<Tester>
        create_tester() const override {
            return std::unique_ptr<Tester>(
                new SingleThreadedTester());
        }

        const char *
        get_type_string() const override {
            return "single_threaded";
        }
    };

    B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **out,
            B_ErrorHandler const *eh) override {
        return b_question_queue_allocate_single_threaded(
            out, eh);
    }

    bool
    consumes_spurious_events() override {
        return true;
    }

    bool
    try_consume_event() override {
        return true;
    }
};

#if defined(B_CONFIG_PTHREAD)
class PthreadCondTester : public Tester {
public:
    class Factory : public Tester::Factory {
    public:
        std::unique_ptr<Tester>
        create_tester() const override {
            return std::unique_ptr<Tester>(
                new PthreadCondTester());
        }


        const char *
        get_type_string() const override {
            return "pthread_cond";
        }
    };

    PthreadCondTester() :
            signaled(false),
            started(false),
            stop(false) {
        int rc;
        rc = pthread_mutex_init(&this->lock, nullptr);
        EXPECT_EQ(0, rc) << strerror(rc);
        rc = pthread_cond_init(&this->cond, nullptr);
        EXPECT_EQ(0, rc) << strerror(rc);

        rc = pthread_create(
            &this->thread,
            nullptr,
            PthreadCondTester::thread_body,
            this);
        EXPECT_EQ(0, rc) << strerror(rc);

        // Wait for thread to say it's started.
        rc = pthread_mutex_lock(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);
        while (!this->started) {
            rc = pthread_cond_wait(
                &this->cond, &this->lock);
            EXPECT_EQ(0, rc) << strerror(rc);
        }
        rc = pthread_mutex_unlock(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);
    }

    ~PthreadCondTester() {
        int rc;

        rc = pthread_mutex_lock(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);
        this->stop = true;
        rc = pthread_cond_signal(&this->cond);
        EXPECT_EQ(0, rc) << strerror(rc);
        rc = pthread_mutex_unlock(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);

        rc = pthread_join(this->thread, nullptr);
        EXPECT_EQ(0, rc) << strerror(rc);

        rc = pthread_cond_destroy(&this->cond);
        EXPECT_EQ(0, rc) << strerror(rc);
        rc = pthread_mutex_destroy(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);
    }

    B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **out,
            B_ErrorHandler const *eh) override {
        return b_question_queue_allocate_with_pthread_cond(
            &this->cond, out, eh);
    }

    bool
    consumes_spurious_events() override {
        return true;
    }

    bool
    try_consume_event() override {
        bool signaled = this->raw_try_consume_event();
        if (!signaled) {
            // Try again after sleeping a bit.
            usleep(20 * 1000);
            signaled = this->raw_try_consume_event();
            if (!signaled) {
                // Once more.
                usleep(1000 * 1000);
                signaled = this->raw_try_consume_event();
            }
        }
        return signaled;
    }

private:
    bool
    raw_try_consume_event() {
        bool signaled;
        int rc;

        rc = pthread_mutex_lock(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);

        signaled = this->signaled;
        this->signaled = false;

        rc = pthread_mutex_unlock(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);

        return signaled;
    }

    static void *
    thread_body(
            void *opaque) {
        int rc;

        PthreadCondTester *self
            = static_cast<PthreadCondTester *>(opaque);
        EXPECT_NE(nullptr, self);

        rc = pthread_mutex_lock(&self->lock);
        EXPECT_EQ(0, rc) << strerror(rc);

        self->started = true;
        rc = pthread_cond_signal(&self->cond);
        EXPECT_EQ(0, rc) << strerror(rc);

        while (!self->stop) {
            rc = pthread_cond_wait(
                &self->cond, &self->lock);
            EXPECT_EQ(0, rc) << strerror(rc);

            // Signal unconditionally.  This is correct
            // because we are testing that the condition
            // variable was signaled.
            self->signaled = true;
        }
        rc = pthread_mutex_unlock(&self->lock);
        EXPECT_EQ(0, rc) << strerror(rc);

        return nullptr;
    }

    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_t thread;
    bool signaled;
    bool started;
    bool stop;
};
#endif

#if defined(B_CONFIG_KQUEUE)
class KqueueTester : public Tester {
public:
    class Factory : public Tester::Factory {
    public:
        std::unique_ptr<Tester>
        create_tester() const override {
            return std::unique_ptr<Tester>(
                new KqueueTester());
        }

        const char *
        get_type_string() const override {
            return "kqueue";
        }
    };

    KqueueTester() {
        this->kqueue = ::kqueue();
        EXPECT_NE(-1, this->kqueue) << strerror(errno);

        struct kevent change;
        EV_SET(
            &change,
            reinterpret_cast<uintptr_t>(this),
            EVFILT_USER,
            EV_ADD,
            NOTE_TRIGGER,
            0,
            nullptr);
        int rc = kevent(
            this->kqueue, &change, 1, nullptr, 0, nullptr);
        EXPECT_EQ(0, rc) << strerror(errno);
    }

    ~KqueueTester() {
        EXPECT_EQ(0, close(this->kqueue))
            << strerror(errno);
    }

    B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **out,
            B_ErrorHandler const *eh) override {
        struct kevent trigger;
        EV_SET(
            &trigger,
            reinterpret_cast<uintptr_t>(this),
            EVFILT_USER,
            EV_ENABLE,
            NOTE_TRIGGER,
            0,
            nullptr);
        return b_question_queue_allocate_with_kqueue(
            this->kqueue, &trigger, out, eh);
    }

    bool
    consumes_spurious_events() override {
        return false;
    }

    bool
    try_consume_event() override {
        const struct timespec timeout = {0, 0};
        struct kevent events[2];
        int rc = kevent(
            this->kqueue,
            nullptr,
            0,
            events,
            sizeof(events) / sizeof(*events),
            &timeout);
        if (rc == -1 && errno == ETIMEDOUT) {
            return false;
        } else if (rc >= 1) {
            EXPECT_EQ(
                this, reinterpret_cast<KqueueTester *>(
                    events[0].ident));
            EXPECT_EQ(1, rc) << "Too many events";
            return true;
        } else {
            EXPECT_NE(-1, rc) << strerror(errno);
            return false;
        }
    }

private:
    int kqueue;
};
#endif

#if defined(B_CONFIG_EVENTFD)
class EventfdTester : public Tester {
public:
    class Factory : public Tester::Factory {
    public:
        std::unique_ptr<Tester>
        create_tester() const override {
            return std::unique_ptr<Tester>(
                new EventfdTester());
        }

        const char *
        get_type_string() const override {
            return "eventfd";
        }
    };

    EventfdTester() {
        this->eventfd = ::eventfd(0, O_CLOEXEC);
        EXPECT_NE(-1, this->eventfd) << strerror(errno);
    }

    ~EventfdTester() {
        EXPECT_EQ(0, close(this->eventfd))
            << strerror(errno);
    }

    B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **out,
            B_ErrorHandler const *eh) override {
        return b_question_queue_allocate_with_eventfd(
            this->eventfd, out, eh);
    }

    bool
    consumes_spurious_events() override {
        return false;
    }

    bool
    try_consume_event() override {
        struct pollfd poll_event;
        poll_event.fd = this->eventfd;
        poll_event.events = POLLIN;

        int rc = poll(&poll_event, 1, 0);
        if (rc >= 1) {
            EXPECT_EQ(1, rc) << "Too many events";
            EXPECT_TRUE(poll_event.revents & POLLIN);
            eventfd_t value;
            EXPECT_EQ(0, eventfd_read(
                this->eventfd, &value)) << strerror(errno);
            EXPECT_EQ(static_cast<eventfd_t>(1), value);
            return true;
        } else if (rc == 0) {
            return false;
        } else {
            EXPECT_EQ(0, rc) << strerror(errno);
            return false;
        }
    }

private:
    int eventfd;
};
#endif

std::ostream &operator<<(
        std::ostream &stream,
        Testers::Tester::Factory *tester_factory) {
    if (tester_factory) {
        stream << tester_factory->get_type_string();
    } else {
        stream << "(null)";
    }
    return stream;
}

Testers::Tester::Factory *tester_factories[] = {
    new Testers::SingleThreadedTester::Factory(),
#if defined(B_CONFIG_PTHREAD)
    new Testers::PthreadCondTester::Factory(),
#endif
#if defined(B_CONFIG_KQUEUE)
    new Testers::KqueueTester::Factory(),
#endif
#if defined(B_CONFIG_EVENTFD)
    new Testers::EventfdTester::Factory(),
#endif
};

}

class TestQuestionQueue : public ::testing::TestWithParam<
        Testers::Tester::Factory *> {
public:
    std::unique_ptr<Testers::Tester>
    create_tester() const {
        return this->GetParam()->create_tester();
    }
};

INSTANTIATE_TEST_CASE_P(
    ,  // No prefix.
    TestQuestionQueue,
    ::testing::ValuesIn(Testers::tester_factories));

TEST_P(TestQuestionQueue, AllocateDeallocate) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_QuestionQueue *queue;
    ASSERT_TRUE(tester->queue_allocate(&queue, eh));
    EXPECT_TRUE(b_question_queue_deallocate(queue, eh));
}

TEST_P(TestQuestionQueue, EnqueueOneItemSignals) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    // Must be alive while queue is destructed.
    StrictMock<MockQuestion> question;
    StrictMock<MockQuestionQueueItem>
        queue_item(&question, &MockQuestion::vtable);
    EXPECT_CALL(queue_item, deallocate(_))
        .WillOnce(Return(true));

    B_QuestionQueue *queue_raw;
    ASSERT_TRUE(tester->queue_allocate(&queue_raw, eh));
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        queue(queue_raw, eh);
    queue_raw = nullptr;

    ASSERT_TRUE(b_question_queue_enqueue(
        queue.get(),
        &queue_item,
        eh));

    EXPECT_TRUE(tester->try_consume_event());
}

TEST_P(TestQuestionQueue, EmptyQueueIsUnsignaled) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_QuestionQueue *queue_raw;
    ASSERT_TRUE(tester->queue_allocate(&queue_raw, eh));
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        queue(queue_raw, eh);
    queue_raw = nullptr;

    if (!tester->consumes_spurious_events()) {
        EXPECT_FALSE(tester->try_consume_event());
    }
}
