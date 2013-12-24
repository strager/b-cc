#ifndef WORKER_H_6BDDD03D_93FC_4FCF_8D50_20EFA7A811B3
#define WORKER_H_6BDDD03D_93FC_4FCF_8D50_20EFA7A811B3

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyRule;
struct B_Broker;
struct B_QuestionVTableList;
struct B_RuleVTable;

B_ERRFUNC
b_worker_work(
    void *context_zmq,
    struct B_Broker const *,
    struct B_QuestionVTableList const *,
    struct B_AnyRule const *,
    struct B_RuleVTable const *);

#ifdef __cplusplus
}
#endif

#endif
