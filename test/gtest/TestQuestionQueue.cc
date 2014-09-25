#include "Mocks.h"

#include <B/QuestionQueue.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

using namespace testing;

class MockQuestionQueueItemObject :
        public B_QuestionQueueItemObject {
public:
    MockQuestionQueueItemObject(
            B_Question *question,
            B_QuestionVTable const *question_vtable) :
            B_QuestionQueueItemObject{
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
            B_QuestionQueueItemObject *queue_item,
            B_ErrorHandler const *eh) {
        return static_cast<MockQuestionQueueItemObject *>(
            queue_item)->deallocate(eh);
    }

    static B_FUNC
    answer_callback_(
            B_TRANSFER B_OPT B_Answer *answer,
            void *opaque,
            B_ErrorHandler const *eh) {
        return static_cast<MockQuestionQueueItemObject *>(
            opaque)->answer_callback(answer, eh);
    }
};

TEST(TestQuestionQueue, AllocateDeallocate) {
    B_ErrorHandler const *eh = nullptr;

    B_QuestionQueue *queue;
    ASSERT_TRUE(b_question_queue_allocate(&queue, eh));
    ASSERT_TRUE(b_question_queue_deallocate(queue, eh));
}

TEST(TestQuestionQueue, EnqueueOneItem) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while queue is destructed.
    StrictMock<MockQuestion> question;
    StrictMock<MockQuestionQueueItemObject>
        queue_item(&question, &MockQuestion::vtable);
    EXPECT_CALL(queue_item, deallocate(_))
        .WillOnce(Return(true));

    B_QuestionQueue *queue_raw;
    ASSERT_TRUE(b_question_queue_allocate(&queue_raw, eh));
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        queue(queue_raw, eh);
    queue_raw = nullptr;

    ASSERT_TRUE(b_question_queue_enqueue(
        queue.get(),
        &queue_item,
        eh));
}
