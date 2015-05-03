#include "Util/TemporaryDirectory.h"

#include <B/Error.h>
#include <B/FileQuestion.h>
#include <B/QuestionAnswer.h>

#include <errno.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <string>

static struct B_IQuestion const *
b_question_from_file_path_(
    std::string const &file_path) {
  return static_cast<struct B_IQuestion const *>(
    static_cast<void const *>(file_path.c_str()));
}

TEST(TestFile, AnswerNonExistentFileReturnsNull) {
  B_TemporaryDirectory temp_dir
    = B_TemporaryDirectory::create();
  std::string file_path
    = temp_dir.path() + "/non_existent_file";

  struct B_IAnswer *answer;
  struct B_Error e;
  ASSERT_TRUE(b_file_question_vtable()->query_answer(
    b_question_from_file_path_(file_path), &answer, &e));
  EXPECT_FALSE(answer);
}

// FIXME(strager): Should we expect EISDIR?
TEST(TestFile, AnswerDirectoryReturnsEPERM) {
  B_TemporaryDirectory temp_dir
    = B_TemporaryDirectory::create();

  struct B_IAnswer *answer;
  struct B_Error e;
  ASSERT_FALSE(b_file_question_vtable()->query_answer(
    b_question_from_file_path_(temp_dir.path()),
    &answer,
    &e));
  EXPECT_EQ(EPERM, e.posix_error);
}

TEST(TestFile, AnswerEmptyFilesHaveSameAnswer) {
  struct B_Error e;

  struct B_QuestionVTable const *question_vtable
    = b_file_question_vtable();

  B_TemporaryDirectory temp_dir
    = B_TemporaryDirectory::create();
  std::string file_path_1
    = temp_dir.path() + "/empty_file_1";
  std::string file_path_2
    = temp_dir.path() + "/empty_file_2";

  {
    FILE *file_1 = fopen(file_path_1.c_str(), "w");
    ASSERT_TRUE(file_1);
    ASSERT_EQ(0, fclose(file_1));

    FILE *file_2 = fopen(file_path_2.c_str(), "w");
    ASSERT_TRUE(file_2);
    ASSERT_EQ(0, fclose(file_2));
  }

  struct B_IAnswer *answer_1;
  ASSERT_TRUE(question_vtable->query_answer(
    b_question_from_file_path_(file_path_1),
    &answer_1,
    &e));

  struct B_IAnswer *answer_2;
  ASSERT_TRUE(question_vtable->query_answer(
    b_question_from_file_path_(file_path_2),
    &answer_2,
    &e));

  ASSERT_TRUE(question_vtable->answer_vtable->equal(
    answer_1, answer_2));

  question_vtable->answer_vtable->deallocate(answer_1);
  question_vtable->answer_vtable->deallocate(answer_2);
}

TEST(
    TestFile,
    AnswerFilesWithDifferingContentsHaveDifferingAnswers) {
  struct B_Error e;

  struct B_QuestionVTable const *question_vtable
    = b_file_question_vtable();

  B_TemporaryDirectory temp_dir
    = B_TemporaryDirectory::create();
  std::string file_path_1 = temp_dir.path() + "/file_1";
  std::string file_path_2 = temp_dir.path() + "/file_2";

  {
    static char const contents_1[] = "hello world";
    FILE *file_1 = fopen(file_path_1.c_str(), "w");
    ASSERT_TRUE(file_1);
    ASSERT_EQ(sizeof(contents_1), fwrite(
      contents_1, 1, sizeof(contents_1), file_1));
    ASSERT_EQ(0, fclose(file_1));

    static char const contents_2[] = "Hello World";
    FILE *file_2 = fopen(file_path_2.c_str(), "w");
    ASSERT_TRUE(file_2);
    ASSERT_EQ(sizeof(contents_2), fwrite(
      contents_2, 1, sizeof(contents_2), file_2));
    ASSERT_EQ(0, fclose(file_2));
  }

  struct B_IAnswer *answer_1;
  ASSERT_TRUE(question_vtable->query_answer(
    b_question_from_file_path_(file_path_1),
    &answer_1,
    &e));

  struct B_IAnswer *answer_2;
  ASSERT_TRUE(question_vtable->query_answer(
    b_question_from_file_path_(file_path_2),
    &answer_2,
    &e));

  ASSERT_FALSE(question_vtable->answer_vtable->equal(
    answer_1, answer_2));

  question_vtable->answer_vtable->deallocate(answer_1);
  question_vtable->answer_vtable->deallocate(answer_2);
}

TEST(TestFile, AnswerFileWithOneOfEveryByte) {
  // This test may seem weird, but there was a bug where
  // answering a file with 0xFF inside returned an error
  // (as (char)-1 (EOF indictation) equals (char)0xFF on
  // some platforms).

  struct B_Error e;

  struct B_QuestionVTable const *question_vtable
    = b_file_question_vtable();

  B_TemporaryDirectory temp_dir
    = B_TemporaryDirectory::create();
  std::string file_path = temp_dir.path() + "/file";

  {
    FILE *file = fopen(file_path.c_str(), "w");
    ASSERT_TRUE(file);
    for (size_t i = 0x00; i <= 0xFF; ++i) {
      uint8_t byte = static_cast<uint8_t>(i);
      ASSERT_EQ(
        static_cast<size_t>(1),
        fwrite(&byte, 1, 1, file));
    }
    ASSERT_EQ(0, fclose(file));
  }

  struct B_IAnswer *answer;
  ASSERT_TRUE(question_vtable->query_answer(
    b_question_from_file_path_(file_path), &answer, &e));
  ASSERT_TRUE(answer);

  question_vtable->answer_vtable->deallocate(answer);
}
