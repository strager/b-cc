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

using namespace testing;

class MockQuestionQueueItem :
        public B_QuestionQueueItem {
public:
    MockQuestionQueueItem(
            B_Question *question,
            B_QuestionVTable const *question_vtable) :
            B_QuestionQueueItem{
                deallocate_,
                question,
                question_vtable,
                answer_callback_} {
    }

    MOCK_METHOD1(deallocate, B_FUNC(
        B_ErrorHandler const *));

    MOCK_METHOD2(answer_callback, B_FUNC(
        B_TRANSFER B_OPT B_Answer *,
        B_ErrorHandler const *));

private:
    static B_FUNC
    deallocate_(
            B_QuestionQueueItem *queue_item,
            B_ErrorHandler const *eh) {
        return static_cast<MockQuestionQueueItem *>(
            queue_item)->deallocate(eh);
    }

    static B_FUNC
    answer_callback_(
            B_TRANSFER B_OPT B_Answer *answer,
            void *opaque,
            B_ErrorHandler const *eh) {
        return static_cast<MockQuestionQueueItem *>(
            opaque)->answer_callback(answer, eh);
    }
};

namespace Testers {

class Tester {
public:
    virtual ~Tester() {
    }

    virtual const char *
    get_type_string() const = 0;

    virtual B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **,
            B_ErrorHandler const *) = 0;

    // Tests if an event was given by the QuestionQueue.  If
    // so, the event is consumed.
    //
    // Spurious wake-ups are possible.  In other words,
    // there are false positives for signaled being true.
    virtual bool
    try_consume_event() = 0;
};

class SingleThreadedTester : public Tester {
public:
    const char *
    get_type_string() const override {
        return "single_threaded";
    }

    B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **out,
            B_ErrorHandler const *eh) override {
        return b_question_queue_allocate_single_threaded(
            out, eh);
    }

    bool
    try_consume_event() override {
        return true;
    }
};

#if defined(B_CONFIG_PTHREAD)
class PthreadCondTester : public Tester {
public:
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
        rc = pthread_cond_destroy(&this->cond);
        EXPECT_EQ(0, rc) << strerror(rc);
        rc = pthread_mutex_destroy(&this->lock);
        EXPECT_EQ(0, rc) << strerror(rc);
    }

    const char *
    get_type_string() const override {
        return "pthread_cond";
    }

    B_FUNC
    queue_allocate(
            B_OUTPTR B_QuestionQueue **out,
            B_ErrorHandler const *eh) override {
        return b_question_queue_allocate_with_pthread_cond(
            &this->cond, out, eh);
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

std::ostream &operator<<(
        std::ostream &stream,
        Testers::Tester *tester) {
    if (tester) {
        stream << tester->get_type_string();
    } else {
        stream << "(null)";
    }
    return stream;
}

Testers::Tester *tester_instances[] = {
    new Testers::SingleThreadedTester(),
#if defined(B_CONFIG_PTHREAD)
    new Testers::PthreadCondTester(),
#endif
};

}

class TestQuestionQueue :
        public ::testing::TestWithParam<Testers::Tester *> {
public:
    Testers::Tester *
    tester() const {
        return this->GetParam();
    }
};

INSTANTIATE_TEST_CASE_P(
    ,  // No prefix.
    TestQuestionQueue,
    ::testing::ValuesIn(Testers::tester_instances));

TEST_P(TestQuestionQueue, AllocateDeallocate) {
    B_ErrorHandler const *eh = nullptr;

    B_QuestionQueue *queue;
    ASSERT_TRUE(this->tester()->queue_allocate(&queue, eh));
    EXPECT_TRUE(b_question_queue_deallocate(queue, eh));
}

TEST_P(TestQuestionQueue, EnqueueOneItemSignals) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while queue is destructed.
    StrictMock<MockQuestion> question;
    StrictMock<MockQuestionQueueItem>
        queue_item(&question, &MockQuestion::vtable);
    EXPECT_CALL(queue_item, deallocate(_))
        .WillOnce(Return(true));

    B_QuestionQueue *queue_raw;
    ASSERT_TRUE(this->tester()->queue_allocate(
        &queue_raw, eh));
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        queue(queue_raw, eh);
    queue_raw = nullptr;

    ASSERT_TRUE(b_question_queue_enqueue(
        queue.get(),
        &queue_item,
        eh));

    EXPECT_TRUE(this->tester()->try_consume_event());
}
