#ifndef BROKER_H_639BF0A4_565A_4687_B404_812552C251E6
#define BROKER_H_639BF0A4_565A_4687_B404_812552C251E6

#include <B/Internal/Common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyDatabase;
struct B_DatabaseVTable;

struct B_BrokerAddress;

B_ERRFUNC
b_broker_address_allocate(
    struct B_BrokerAddress **out);

B_ERRFUNC
b_broker_address_deallocate(
    struct B_BrokerAddress *);

B_ERRFUNC
b_broker_run(
    // TODO(strager): Swap first two params.
    struct B_BrokerAddress const *,
    void *context_zmq,
    struct B_AnyDatabase *,
    struct B_DatabaseVTable const *);

#ifdef __cplusplus
}
#endif

#endif
