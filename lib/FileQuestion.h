#ifndef FILEQUESTION_H_B6A426AD_5F9A_4057_9C0A_4BDFF463E4A8
#define FILEQUESTION_H_B6A426AD_5F9A_4057_9C0A_4BDFF463E4A8

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyQuestion;

struct B_AnyQuestion *
b_file_question_allocate(
    const char *file_path);

void
b_file_question_deallocate(
    struct B_AnyQuestion *);

const char *
b_file_question_file_path(
    const struct B_AnyQuestion *);

const struct B_QuestionVTable *
b_file_question_vtable();

#ifdef __cplusplus
}
#endif

#endif
