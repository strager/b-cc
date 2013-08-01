#include "Allocate.h"
#include "Answer.h"
#include "Exception.h"
#include "FileQuestion.h"
#include "Portable.h"
#include "Question.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct B_AnyQuestion *
b_file_question_allocate(
    const char *file_path) {
    return (struct B_AnyQuestion *) b_strdup(file_path);
}

const char *
b_file_question_file_path(
    const struct B_AnyQuestion *question) {
    return (const char *) question;
}

struct FileAnswer {
    struct timespec modified_time;
};

static struct B_AnyAnswer *
b_file_answer_allocate(
    const struct timespec *modified_time) {
    B_ALLOCATE(struct FileAnswer, answer, {
        .modified_time = *modified_time,
    });
    return (struct B_AnyAnswer *) answer;
}

static struct B_AnyAnswer *
b_file_question_answer(
    const struct B_AnyQuestion *question,
    struct B_Exception **ex) {
    const char *file_path
        = b_file_question_file_path(question);

    struct stat stat;
    int err = lstat(file_path, &stat);
    if (err < 0) {
        *ex = b_exception_errno("lstat", errno);
        return NULL;
    }
    return b_file_answer_allocate(&stat.st_mtimespec);
}

static bool
b_file_question_equal(
    const struct B_AnyQuestion *a,
    const struct B_AnyQuestion *b) {
    return strcmp(
        b_file_question_file_path(a),
        b_file_question_file_path(b)) == 0;
}

static struct B_AnyQuestion *
b_file_question_replicate(
    const struct B_AnyQuestion *question) {
    return b_file_question_allocate(
        b_file_question_file_path(question));
}

void
b_file_question_deallocate(
    struct B_AnyQuestion *question) {
    free((char *) question);
}

static bool
b_file_answer_equal(
    const struct B_AnyAnswer *a_answer,
    const struct B_AnyAnswer *b_answer) {
    struct FileAnswer *a = (struct FileAnswer *) a_answer;
    struct FileAnswer *b = (struct FileAnswer *) b_answer;
    return a->modified_time.tv_nsec == b->modified_time.tv_nsec
        && a->modified_time.tv_sec == b->modified_time.tv_sec;
}

struct B_AnyAnswer *
b_file_answer_replicate(
    const struct B_AnyAnswer *answer) {
    return b_file_answer_allocate(
        &((struct FileAnswer *) answer)->modified_time);
}

void
b_file_answer_deallocate(
    struct B_AnyAnswer *answer) {
    free(answer);
}

static const struct B_AnswerVTable *
b_file_answer_vtable() {
    static const struct B_AnswerVTable vtable = {
        .equal = b_file_answer_equal,
        .replicate = b_file_answer_replicate,
        .deallocate = b_file_answer_deallocate,
    };
    return &vtable;
}

const struct B_QuestionVTable *
b_file_question_vtable() {
    static struct B_QuestionVTable vtable = {
        .uuid = B_UUID("F90D4C63-397C-4C93-B3F0-0F2A1CFFE55B"),
        .answer_vtable = NULL,
        .answer = b_file_question_answer,
        .equal = b_file_question_equal,
        .replicate = b_file_question_replicate,
        .deallocate = b_file_question_deallocate,
    };
    vtable.answer_vtable = b_file_answer_vtable();
    return &vtable;
}
