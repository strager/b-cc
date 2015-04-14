#include "Mocks.h"
#include "Util.h"

#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionQueue.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Ensures b_answer_context_need_one enqueues the question
// on the QuestionQueue exactly once.
TEST(TestAnswerContext, NeedOneEnqueues) {
    B_ErrorHandler const *eh = nullptr;

    //bool closed;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question;
    MockRefCounting(needed_question);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) -> bool {
        return true;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question,
        &MockQuestion::vtable,
        [](
                B_TRANSFER B_Answer *,
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        [](
                B_ErrorHandler const *) -> bool {
            return true;
        },
        eh));

#if 0
    B_QuestionQueueItem *first_queue_item;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue, &first_queue_item, &closed, eh));
    ASSERT_NE(nullptr, first_queue_item);
    EXPECT_FALSE(closed);

    EXPECT_EQ(&needed_question, first_queue_item->question);
    EXPECT_EQ(
        &MockQuestion::vtable,
        first_queue_item->question_vtable);

    EXPECT_TRUE(b_question_queue_item_object_deallocate(
        first_queue_item, eh));

    B_QuestionQueueItem *second_queue_item;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue, &second_queue_item, &closed, eh));
    ASSERT_EQ(nullptr, second_queue_item);
    EXPECT_FALSE(closed);
#endif

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}

// Ensures calling b_answer_context_need_one's enqueued
// item answer_callback with an answer calls the
// b_answer_context_need_one success callback.
TEST(TestAnswerContext, AnswerSuccessCallsNeedCallback) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question;
    MockRefCounting(needed_question);
    StrictMock<MockAnswer> answer;
    MockRefCounting(answer);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) -> bool {
        return true;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    size_t need_callback_called = 0;
    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question,
        &MockQuestion::vtable,
        [&](
                B_TRANSFER B_Answer *cur_answer,
                B_ErrorHandler const *cur_eh) -> bool {
            need_callback_called += 1;
            EXPECT_EQ(&answer, cur_answer);
            EXPECT_EQ(eh, cur_eh);
            return true;
        },
        [](
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        eh));

    bool closed;
    B_QuestionQueueItem *queue_item;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue, &queue_item, &closed, eh));
    ASSERT_NE(nullptr, queue_item);
    EXPECT_FALSE(closed);

    EXPECT_EQ(&needed_question, queue_item->question);
    EXPECT_EQ(
        &MockQuestion::vtable, queue_item->question_vtable);
    ASSERT_NE(nullptr, queue_item->answer_callback);
    EXPECT_EQ(0U, need_callback_called);

    ASSERT_TRUE(B_RETAIN(&answer, eh));
    ASSERT_TRUE(queue_item->answer_callback(
        &answer, queue_item, eh));
    EXPECT_EQ(1U, need_callback_called);

    EXPECT_TRUE(b_question_queue_item_object_deallocate(
        queue_item, eh));

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}

// Ensures calling the enqueued item answer_callback-s of
// two b_answer_context_need_one calls with an answer calls
// the b_answer_context_need_one success callback.
TEST(
        TestAnswerContext,
        AnswerSuccessSuccessCallsNeedCallbacks) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question_1;
    MockRefCounting(needed_question_1);
    StrictMock<MockQuestion> needed_question_2;
    MockRefCounting(needed_question_2);
    StrictMock<MockAnswer> answer_1;
    MockRefCounting(answer_1);
    StrictMock<MockAnswer> answer_2;
    MockRefCounting(answer_2);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) -> bool {
        return true;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    size_t need_callback_1_called = 0;
    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question_1,
        &MockQuestion::vtable,
        [&](
                B_TRANSFER B_Answer *cur_answer,
                B_ErrorHandler const *cur_eh) -> bool {
            need_callback_1_called += 1;
            EXPECT_EQ(&answer_1, cur_answer);
            EXPECT_EQ(eh, cur_eh);
            return true;
        },
        [](
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        eh));

    size_t need_callback_2_called = 0;
    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question_2,
        &MockQuestion::vtable,
        [&](
                B_TRANSFER B_Answer *cur_answer,
                B_ErrorHandler const *cur_eh) -> bool {
            need_callback_2_called += 1;
            EXPECT_EQ(&answer_2, cur_answer);
            EXPECT_EQ(eh, cur_eh);
            return true;
        },
        [](
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        eh));

    {
        bool closed;
        B_QuestionQueueItem *queue_item_1;
        ASSERT_TRUE(b_question_queue_try_dequeue(
            question_queue, &queue_item_1, &closed, eh));
        ASSERT_NE(nullptr, queue_item_1);
        EXPECT_FALSE(closed);

        EXPECT_EQ(
            &MockQuestion::vtable,
            queue_item_1->question_vtable);
        ASSERT_NE(nullptr, queue_item_1->answer_callback);

        B_Answer *queue_item_1_answer;
        size_t *queue_item_1_need_callback_called;
        if (queue_item_1->question == &needed_question_1) {
            queue_item_1_answer = &answer_1;
            ASSERT_TRUE(B_RETAIN(&answer_1, eh));
            queue_item_1_need_callback_called
                = &need_callback_1_called;
        } else if (queue_item_1->question
                == &needed_question_2) {
            queue_item_1_answer = &answer_2;
            ASSERT_TRUE(B_RETAIN(&answer_2, eh));
            queue_item_1_need_callback_called
                = &need_callback_2_called;
        } else {
            FAIL();
        }
        EXPECT_EQ(0U, *queue_item_1_need_callback_called);
        ASSERT_TRUE(queue_item_1->answer_callback(
            queue_item_1_answer, queue_item_1, eh));
        EXPECT_EQ(1U, *queue_item_1_need_callback_called);

        EXPECT_TRUE(b_question_queue_item_object_deallocate(
            queue_item_1, eh));
    }

    {
        bool closed;
        B_QuestionQueueItem *queue_item_2;
        ASSERT_TRUE(b_question_queue_try_dequeue(
            question_queue, &queue_item_2, &closed, eh));
        ASSERT_NE(nullptr, queue_item_2);
        EXPECT_FALSE(closed);

        EXPECT_EQ(
            &MockQuestion::vtable,
            queue_item_2->question_vtable);
        ASSERT_NE(nullptr, queue_item_2->answer_callback);

        B_Answer *queue_item_2_answer;
        size_t *queue_item_2_need_callback_called;
        if (queue_item_2->question == &needed_question_1) {
            queue_item_2_answer = &answer_1;
            ASSERT_TRUE(B_RETAIN(&answer_1, eh));
            queue_item_2_need_callback_called
                = &need_callback_1_called;
        } else if (queue_item_2->question
                == &needed_question_2) {
            queue_item_2_answer = &answer_2;
            ASSERT_TRUE(B_RETAIN(&answer_2, eh));
            queue_item_2_need_callback_called
                = &need_callback_2_called;
        } else {
            FAIL();
        }
        EXPECT_EQ(0U, *queue_item_2_need_callback_called);
        ASSERT_TRUE(queue_item_2->answer_callback(
            queue_item_2_answer, queue_item_2, eh));
        EXPECT_EQ(1U, *queue_item_2_need_callback_called);

        EXPECT_TRUE(b_question_queue_item_object_deallocate(
            queue_item_2, eh));
    }

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}

// Ensures calling the enqueued item answer_callback-s of a
// b_answer_context_need call with an answer calls the
// b_answer_context_need success callback.
TEST(
        TestAnswerContext,
        AnswerSuccessSuccessCallsNeedCallback) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question_1;
    MockRefCounting(needed_question_1);
    StrictMock<MockQuestion> needed_question_2;
    MockRefCounting(needed_question_2);
    StrictMock<MockAnswer> answer_1;
    MockRefCounting(answer_1);
    StrictMock<MockAnswer> answer_2;
    MockRefCounting(answer_2);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) -> bool {
        return true;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    B_Question const *needed_questions[2] = {
        &needed_question_1,
        &needed_question_2,
    };
    B_QuestionVTable const *needed_question_vtables[2] = {
        &MockQuestion::vtable,
        &MockQuestion::vtable,
    };

    size_t need_callback_called = 0;
    ASSERT_TRUE(b_answer_context_need(
        &answer_context,
        needed_questions,
        needed_question_vtables,
        2,
        [&](
                B_TRANSFER B_Answer *const *cur_answers,
                B_ErrorHandler const *cur_eh) -> bool {
            need_callback_called += 1;
            EXPECT_EQ(&answer_1, cur_answers[0]);
            EXPECT_EQ(&answer_2, cur_answers[1]);
            EXPECT_EQ(eh, cur_eh);
            return true;
        },
        [](
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        eh));

    {
        bool closed;
        B_QuestionQueueItem *queue_item_1;
        ASSERT_TRUE(b_question_queue_try_dequeue(
            question_queue, &queue_item_1, &closed, eh));
        ASSERT_NE(nullptr, queue_item_1);
        EXPECT_FALSE(closed);

        EXPECT_EQ(
            &MockQuestion::vtable,
            queue_item_1->question_vtable);
        ASSERT_NE(nullptr, queue_item_1->answer_callback);

        B_Answer *queue_item_1_answer;
        if (queue_item_1->question == &needed_question_1) {
            queue_item_1_answer = &answer_1;
            ASSERT_TRUE(B_RETAIN(&answer_1, eh));
        } else if (queue_item_1->question
                == &needed_question_2) {
            queue_item_1_answer = &answer_2;
            ASSERT_TRUE(B_RETAIN(&answer_2, eh));
        } else {
            FAIL();
        }
        EXPECT_EQ(0U, need_callback_called);
        ASSERT_TRUE(queue_item_1->answer_callback(
            queue_item_1_answer, queue_item_1, eh));
        EXPECT_EQ(0U, need_callback_called);

        EXPECT_TRUE(b_question_queue_item_object_deallocate(
            queue_item_1, eh));
    }

    {
        bool closed;
        B_QuestionQueueItem *queue_item_2;
        ASSERT_TRUE(b_question_queue_try_dequeue(
            question_queue, &queue_item_2, &closed, eh));
        ASSERT_NE(nullptr, queue_item_2);
        EXPECT_FALSE(closed);

        EXPECT_EQ(
            &MockQuestion::vtable,
            queue_item_2->question_vtable);
        ASSERT_NE(nullptr, queue_item_2->answer_callback);

        B_Answer *queue_item_2_answer;
        if (queue_item_2->question == &needed_question_1) {
            queue_item_2_answer = &answer_1;
            ASSERT_TRUE(B_RETAIN(&answer_1, eh));
        } else if (queue_item_2->question
                == &needed_question_2) {
            queue_item_2_answer = &answer_2;
            ASSERT_TRUE(B_RETAIN(&answer_2, eh));
        } else {
            FAIL();
        }
        EXPECT_EQ(0U, need_callback_called);
        ASSERT_TRUE(queue_item_2->answer_callback(
            queue_item_2_answer, queue_item_2, eh));
        EXPECT_EQ(1U, need_callback_called);

        EXPECT_TRUE(b_question_queue_item_object_deallocate(
            queue_item_2, eh));
    }

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}

// Ensures calling b_answer_context_need_one's enqueued
// item answer_callback without an answer calls the
// b_answer_context_need_one error callback.
TEST(TestAnswerContext, AnswerErrorCallsNeedCallback) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question;
    MockRefCounting(needed_question);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) -> bool {
        return true;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    size_t need_callback_called = 0;
    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question,
        &MockQuestion::vtable,
        [](
                B_TRANSFER B_Answer *,
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        [&](
                B_ErrorHandler const *cur_eh) -> bool {
            need_callback_called += 1;
            EXPECT_EQ(eh, cur_eh);
            return true;
        },
        eh));

    bool closed;
    B_QuestionQueueItem *queue_item;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue, &queue_item, &closed, eh));
    ASSERT_NE(nullptr, queue_item);
    EXPECT_FALSE(closed);

    EXPECT_EQ(&needed_question, queue_item->question);
    EXPECT_EQ(
        &MockQuestion::vtable, queue_item->question_vtable);
    ASSERT_NE(nullptr, queue_item->answer_callback);
    EXPECT_EQ(0U, need_callback_called);

    ASSERT_TRUE(queue_item->answer_callback(
        nullptr, queue_item, eh));
    EXPECT_EQ(1U, need_callback_called);

    EXPECT_TRUE(b_question_queue_item_object_deallocate(
        queue_item, eh));

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}

// Ensures calling b_answer_context_success_answer calls the
// AnswerContext's answer_callback with the answer.
TEST(TestAnswerContext, SuccessAnswerCallsContextCallback) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockAnswer> answer;
    MockRefCounting(answer);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    size_t answer_callback_called = 0;
    auto answer_callback = [&](
            B_Answer *cur_answer,
            B_ErrorHandler const *cur_eh) -> bool {
        answer_callback_called += 1;
        EXPECT_EQ(&answer, cur_answer);
        EXPECT_EQ(eh, cur_eh);
        return true;
    };
    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *answer,
            void *opaque,
            B_ErrorHandler const *eh) -> bool {
        return (*static_cast<decltype(answer_callback) *>(
            opaque))(answer, eh);
    };
    answer_context.answer_callback_opaque
        = &answer_callback;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    EXPECT_EQ(0U, answer_callback_called);
    ASSERT_TRUE(b_answer_context_success_answer(
        &answer_context,
        &answer,
        eh));
    EXPECT_EQ(1U, answer_callback_called);

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}

// Ensures calling b_answer_context_success_answer calls the
// AnswerContext's answer_callback with the answer.
TEST(TestAnswerContext, SuccessCallsContextCallback) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockAnswer> answer;
    MockRefCounting(answer);
    B_AnswerContext answer_context;
    EXPECT_CALL(question, query_answer(_, _))
        .WillOnce(DoAll(
            SetArgPointee<0>(&answer),
            Return(true)));

    B_QuestionQueue *question_queue;
    ASSERT_TRUE(b_question_queue_allocate_single_threaded(
        &question_queue, eh));

    size_t answer_callback_called = 0;
    auto answer_callback = [&](
            B_Answer *cur_answer,
            B_ErrorHandler const *cur_eh) -> bool {
        answer_callback_called += 1;
        EXPECT_EQ(&answer, cur_answer);
        EXPECT_EQ(eh, cur_eh);
        return true;
    };
    answer_context.question = &question;
    answer_context.question_vtable = &MockQuestion::vtable;
    answer_context.answer_callback = [](
            B_Answer *answer,
            void *opaque,
            B_ErrorHandler const *eh) -> bool {
        return (*static_cast<decltype(answer_callback) *>(
            opaque))(answer, eh);
    };
    answer_context.answer_callback_opaque
        = &answer_callback;
    answer_context.question_queue = question_queue;
    answer_context.database = nullptr;

    EXPECT_EQ(0U, answer_callback_called);
    ASSERT_TRUE(b_answer_context_success(
        &answer_context,
        eh));
    EXPECT_EQ(1U, answer_callback_called);

    EXPECT_TRUE(b_question_queue_deallocate(
        question_queue, eh));
}
