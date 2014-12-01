#include "Mocks.h"

#include <B/AnswerContext.h>
#include <B/Database.h>
#include <B/QuestionDispatch.h>
#include <B/QuestionQueue.h>

#include <gtest/gtest.h>
#include <sqlite3.h>

TEST(TestQuestionDispatch, EmptyDatabaseSuccess) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestionQueueItem> queue_item(
        &question, &MockQuestion::vtable);

    B_QuestionQueue *queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &queue, eh));

    B_Database *database;
    ASSERT_TRUE(b_database_load_sqlite(
        ":memory:",
        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
        nullptr,
        &database,
        eh));

    EXPECT_CALL(question, serialize(_, _))
        .WillRepeatedly(Return(true));
    B_AnswerContext const *answer_context;
    ASSERT_TRUE(b_question_dispatch_one(
        &queue_item, queue, database, &answer_context, eh));
    ASSERT_NE(nullptr, answer_context);

    StrictMock<MockAnswer> answer;
    MockRefCounting(answer);
    EXPECT_CALL(answer, serialize(_, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(queue_item, answer_callback(&answer, _))
        .WillOnce(Return(true));
    EXPECT_CALL(queue_item, deallocate(_))
        .WillOnce(Return(true));
    EXPECT_TRUE(b_answer_context_success_answer(
        answer_context, &answer, eh));

    EXPECT_TRUE(b_database_close(database, eh));
    EXPECT_TRUE(b_question_queue_deallocate(queue, eh));
}

TEST(TestQuestionDispatch, FailCallbackCalledImmediately) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestionQueueItem> queue_item(
        &question, &MockQuestion::vtable);

    B_QuestionQueue *queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &queue, eh));

    B_Database *database;
    ASSERT_TRUE(b_database_load_sqlite(
        ":memory:",
        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
        nullptr,
        &database,
        eh));

    EXPECT_CALL(question, serialize(_, _))
        .WillRepeatedly(Return(true));
    B_AnswerContext const *answer_context;
    ASSERT_TRUE(b_question_dispatch_one(
        &queue_item, queue, database, &answer_context, eh));
    ASSERT_NE(nullptr, answer_context);

    EXPECT_CALL(queue_item, answer_callback(nullptr, _))
        .WillOnce(Return(true));
    EXPECT_CALL(queue_item, deallocate(_))
        .WillOnce(Return(true));
    EXPECT_TRUE(b_answer_context_error(answer_context, eh));

    EXPECT_TRUE(b_database_close(database, eh));
    EXPECT_TRUE(b_question_queue_deallocate(queue, eh));
}
