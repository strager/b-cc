#ifndef FILEQUESTION_H_B6A426AD_5F9A_4057_9C0A_4BDFF463E4A8
#define FILEQUESTION_H_B6A426AD_5F9A_4057_9C0A_4BDFF463E4A8

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyQuestion;

struct B_AnyQuestion *
b_file_question_constant_string(
    const char *file_path);

const struct B_QuestionVTable *
b_file_question_constant_string_vtable();

#ifdef __cplusplus
}
#endif

#endif
