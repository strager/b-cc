#ifndef CLIENT_H_D074DEDB_276B_4567_A269_B65D4219C265
#define CLIENT_H_D074DEDB_276B_4567_A269_B65D4219C265

#include "Common.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyAnswer;
struct B_AnyQuestion;
struct B_Broker;
struct B_QuestionVTable;

// Clients are *not* thread-safe.  Each client must be
// allocated, deallocated, and used from only one thread.
// (However, multiple clients may be used on one thread.)

struct B_Client;

B_ERRFUNC
b_client_allocate_connect(
    void *context_zmq,
    struct B_Broker const *,
    struct B_Client **out);

B_ERRFUNC
b_client_deallocate_disconnect(
    struct B_Client *);

// Requests some Questions be answered.  Uses the Rule
// registered with the Workers for performing side effects.
// May run Rules in parallel if multiple questions are
// asked.  Returns Answers corresponding to the Questions
// asked.  After calling, the Answers are owned by the
// caller.
B_ERRFUNC
b_client_need_answers(
    struct B_Client *,
    struct B_AnyQuestion const *const *,
    struct B_QuestionVTable const *const *,
    struct B_AnyAnswer **,
    size_t count);

// Like b_client_need_answers, but without returning
// the answers.
B_ERRFUNC
b_client_need(
    struct B_Client *,
    struct B_AnyQuestion const *const *,
    struct B_QuestionVTable const *const *,
    size_t count);

B_ERRFUNC
b_client_need_answer_one(
    struct B_Client *,
    struct B_AnyQuestion const *,
    struct B_QuestionVTable const *,
    struct B_AnyAnswer **out);

B_ERRFUNC
b_client_need_one(
    struct B_Client *,
    struct B_AnyQuestion const *,
    struct B_QuestionVTable const *);

#ifdef __cplusplus
}
#endif

#endif
