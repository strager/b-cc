#include "Mocks.h"
#include "Util.h"

#include <B/AnswerContext.h>
#include <B/Database.h>
#include <B/Main.h>
#include <B/QuestionVTableSet.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sqlite3.h>

TEST(TestMain, AllocateDeallocate) {
    B_ErrorHandler const *eh = nullptr;

    B_Main *main;
    ASSERT_TRUE(b_main_allocate(&main, eh));
    ASSERT_TRUE(b_main_deallocate(main, eh));
}

TEST(TestMain, CallbackCalledForInitialQuestion) {
    B_ErrorHandler const *eh = nullptr;

    B_Database *database;
    ASSERT_TRUE(b_database_load_sqlite(
        ":memory:",
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr,
        &database,
        eh));
    B_QuestionVTableSet *vtable_set;
    ASSERT_TRUE(b_question_vtable_set_allocate(
        &vtable_set, eh));

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

    B_Main *main;
    ASSERT_TRUE(b_main_allocate(&main, eh));
    Closure closure{&answer, 0};
    B_Answer *returned_answer;
    ASSERT_TRUE(b_main_loop(
        main,
        &question,
        &MockQuestion::vtable,
        database,
        &returned_answer,
        [](
                struct B_AnswerContext const *
                    answer_context,
                void *opaque,
                struct B_ErrorHandler const *eh) -> bool {
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
    ASSERT_TRUE(b_database_close(database, eh));
}
