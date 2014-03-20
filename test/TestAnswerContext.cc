#include "Mocks.h"

#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionQueue.h>

#include <memory>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Ensures b_answer_context_need_one enqueues the question
// on the QuestionQueue exactly once.
TEST(TestAnswerContext, NeedOneEnqueues) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question;
    MockRefCounting(needed_question);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue_raw;
    ASSERT_TRUE(b_question_queue_allocate(
        &question_queue_raw,
        eh));
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        question_queue(question_queue_raw, eh);
    question_queue_raw = nullptr;

    answer_context.question = &question;
    answer_context.question_vtable = MockQuestion::vtable();
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) {
        // TODO(strager)
        return false;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue.get();
    answer_context.dependency_delegate = nullptr;

    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question,
        MockQuestion::vtable(),
        [](
                B_TRANSFER B_Answer *,
                B_ErrorHandler const *) {
            return false;
        },
        [](
                B_ErrorHandler const *) {
            return false;
        },
        eh));

    B_QuestionQueueItemObject *first_queue_item_raw;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue.get(),
        &first_queue_item_raw,
        eh));
    ASSERT_NE(nullptr, first_queue_item_raw);
    std::unique_ptr<
            B_QuestionQueueItemObject,
            B_QuestionQueueItemDeleter>
        first_queue_item(first_queue_item_raw, eh);
    first_queue_item_raw = nullptr;

    EXPECT_EQ(&needed_question, first_queue_item->question);
    EXPECT_EQ(
        MockQuestion::vtable(),
        first_queue_item->question_vtable);

    B_QuestionQueueItemObject *second_queue_item_raw;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue.get(),
        &second_queue_item_raw,
        eh));
    ASSERT_EQ(nullptr, second_queue_item_raw);
}
