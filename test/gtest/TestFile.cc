#include "Mocks.h"
#include "Util.h"

#include <B/File.h>
#include <B/Error.h>

#include <cstdint>
#include <cstdio>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

using namespace testing;

static B_Question const *
question_from_file_path_(
        std::string const &file_path) {
    return static_cast<B_Question const *>(
        static_cast<void const *>(file_path.c_str()));
}

TEST(TestFile, AnswerNonExistentFileReturnsNull) {
    auto temp_dir = B_TemporaryDirectory::create();
    std::string file_path
        = temp_dir.path() + "/non_existent_file";

    MockErrorHandler mock_eh;

    B_Answer *answer;
    ASSERT_TRUE(b_file_contents_question_vtable()
        ->query_answer(
            question_from_file_path_(file_path),
            &answer,
            &mock_eh));
    EXPECT_EQ(nullptr, answer);
}

TEST(TestFile, AnswerDirectoryReturnsEISDIR) {
    auto temp_dir = B_TemporaryDirectory::create();

    MockErrorHandler mock_eh;
    EXPECT_CALL(mock_eh, handle_error(B_Error{EISDIR}))
        .WillOnce(Return(B_ERROR_ABORT));

    B_Answer *answer;
    EXPECT_FALSE(b_file_contents_question_vtable()
        ->query_answer(
            question_from_file_path_(temp_dir.path()),
            &answer,
            &mock_eh));
}

TEST(TestFile, AnswerEmptyFilesHaveSameAnswer) {
    B_ErrorHandler const *eh = nullptr;

    B_QuestionVTable const *question_vtable
        = b_file_contents_question_vtable();

    auto temp_dir = B_TemporaryDirectory::create();
    std::string file_path_1
        = temp_dir.path() + "/empty_file_1";
    std::string file_path_2
        = temp_dir.path() + "/empty_file_2";

    {
        FILE *file_1 = fopen(file_path_1.c_str(), "w");
        ASSERT_NE(nullptr, file_1);
        ASSERT_EQ(0, fclose(file_1));

        FILE *file_2 = fopen(file_path_2.c_str(), "w");
        ASSERT_NE(nullptr, file_2);
        ASSERT_EQ(0, fclose(file_2));
    }

    B_Answer *answer_1;
    ASSERT_TRUE(question_vtable->query_answer(
        question_from_file_path_(file_path_1),
        &answer_1,
        eh));

    B_Answer *answer_2;
    ASSERT_TRUE(question_vtable->query_answer(
        question_from_file_path_(file_path_2),
        &answer_2,
        eh));

    bool equal;
    ASSERT_TRUE(question_vtable->answer_vtable
        ->equal(answer_1, answer_2, &equal, eh));
    EXPECT_TRUE(equal);

    ASSERT_TRUE(question_vtable->answer_vtable->deallocate(
        answer_1, eh));
    ASSERT_TRUE(question_vtable->answer_vtable->deallocate(
        answer_2, eh));
}

TEST(TestFile, AnswerFilesWithDifferingContentsHaveDifferingAnswers) {
    B_ErrorHandler const *eh = nullptr;

    B_QuestionVTable const *question_vtable
        = b_file_contents_question_vtable();

    auto temp_dir = B_TemporaryDirectory::create();
    std::string file_path_1
        = temp_dir.path() + "/file_1";
    std::string file_path_2
        = temp_dir.path() + "/file_2";

    {
        static char const contents_1[] = "hello world";
        FILE *file_1 = fopen(file_path_1.c_str(), "w");
        ASSERT_NE(nullptr, file_1);
        ASSERT_EQ(sizeof(contents_1), fwrite(
            contents_1,
            1,
            sizeof(contents_1),
            file_1));
        ASSERT_EQ(0, fclose(file_1));

        static char const contents_2[] = "Hello World";
        FILE *file_2 = fopen(file_path_2.c_str(), "w");
        ASSERT_NE(nullptr, file_2);
        ASSERT_EQ(sizeof(contents_2), fwrite(
            contents_2,
            1,
            sizeof(contents_2),
            file_2));
        ASSERT_EQ(0, fclose(file_2));
    }

    B_Answer *answer_1;
    ASSERT_TRUE(question_vtable->query_answer(
        question_from_file_path_(file_path_1),
        &answer_1,
        eh));

    B_Answer *answer_2;
    ASSERT_TRUE(question_vtable->query_answer(
        question_from_file_path_(file_path_2),
        &answer_2,
        eh));

    bool equal;
    ASSERT_TRUE(question_vtable->answer_vtable
        ->equal(answer_1, answer_2, &equal, eh));
    EXPECT_FALSE(equal);

    ASSERT_TRUE(question_vtable->answer_vtable->deallocate(
        answer_1, eh));
    ASSERT_TRUE(question_vtable->answer_vtable->deallocate(
        answer_2, eh));
}

TEST(TestFile, AnswerFileWithOneOfEveryByte) {
    // This test may seem weird, but there was a bug where
    // answering a file with 0xFF inside returned an error
    // (as (char)-1 (EOF indictation) equals (char)0xFF on
    // some platforms).

    B_ErrorHandler const *eh = nullptr;

    B_QuestionVTable const *question_vtable
        = b_file_contents_question_vtable();

    auto temp_dir = B_TemporaryDirectory::create();
    std::string file_path = temp_dir.path() + "/file";

    {
        FILE *file = fopen(file_path.c_str(), "w");
        ASSERT_NE(nullptr, file);
        for (size_t i = 0; i <= UINT8_MAX; ++i) {
            uint8_t byte = static_cast<uint8_t>(i);
            ASSERT_EQ(
                static_cast<size_t>(1),
                fwrite(&byte, 1, 1, file));
        }
        ASSERT_EQ(0, fclose(file));
    }

    B_Answer *answer;
    ASSERT_TRUE(question_vtable->query_answer(
        question_from_file_path_(file_path),
        &answer,
        eh));
    ASSERT_NE(nullptr, answer);

    ASSERT_TRUE(question_vtable->answer_vtable->deallocate(
        answer, eh));
}
