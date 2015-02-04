#include "Mocks.h"
#include "Util.h"

#include <B/AnswerContext.h>
#include <B/Main.h>
#include <B/QuestionVTableSet.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(TestMain, AllocateDeallocate) {
    B_ErrorHandler const *eh = nullptr;

    B_QuestionVTableSet *vtable_set;
    ASSERT_TRUE(b_question_vtable_set_allocate(
        &vtable_set, eh));

    B_Main *main;
    ASSERT_TRUE(b_main_allocate(
        ":memory:", vtable_set, &main, eh));
    ASSERT_TRUE(b_main_deallocate(main, eh));

    ASSERT_TRUE(b_question_vtable_set_deallocate(
        vtable_set, eh));
}

TEST(TestMain, CallbackCalledForInitialQuestion) {
    B_ErrorHandler const *eh = nullptr;

    B_QuestionVTableSet *vtable_set;
    ASSERT_TRUE(b_question_vtable_set_allocate(
        &vtable_set, eh));

    B_Main *main;
    ASSERT_TRUE(b_main_allocate(
        ":memory:", vtable_set, &main, eh));

    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    EXPECT_CALL(question, serialize(_, _))
        .WillRepeatedly(Return(true));

    StrictMock<MockAnswer> answer;
    MockRefCounting(answer);
    EXPECT_CALL(answer, serialize(_, _))
        .WillRepeatedly(Return(true));

    struct Closure {
        B_Answer *answer;
        size_t called_count;
    };

    Closure closure{&answer, 0};
    B_Answer *returned_answer;
    ASSERT_TRUE(b_main_loop(
        main,
        &question,
        &MockQuestion::vtable,
        &returned_answer,
        [](
                struct B_AnswerContext const *
                    answer_context,
                void *opaque,
                struct B_ErrorHandler const *eh) {
            auto &closure
                = *static_cast<Closure *>(opaque);
            closure.called_count += 1;
            if (!b_answer_context_success_answer(
                    answer_context, closure.answer, eh)) {
                ADD_FAILURE();
                return false;
            }
            return true;
        },
        &closure,
        eh));
    EXPECT_EQ(static_cast<size_t>(1), closure.called_count);

    ASSERT_TRUE(b_main_deallocate(main, eh));
    ASSERT_TRUE(b_question_vtable_set_deallocate(
        vtable_set, eh));
}
