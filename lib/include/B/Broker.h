#ifndef BROKER_H_639BF0A4_565A_4687_B404_812552C251E6
#define BROKER_H_639BF0A4_565A_4687_B404_812552C251E6

#include <B/Internal/Common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyDatabase;
struct B_DatabaseVTable;

struct B_Broker;

// Creates a new Broker, saving state into the given
// Database.  Does not take ownership of the given Database.
B_ERRFUNC
b_broker_allocate_bind(
    void *context_zmq,
    struct B_AnyDatabase *,
    struct B_DatabaseVTable const *,
    struct B_Broker **out);

struct B_Exception *
b_broker_deallocate_unbind(
    struct B_Broker *);

B_ERRFUNC
b_broker_run(
    struct B_Broker *);

#ifdef __cplusplus
}
#endif

#endif
