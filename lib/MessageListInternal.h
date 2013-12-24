#ifndef MESSAGELISTINTERNAL_H_116E8008_B367_4F5C_923C_14993AD2E62E
#define MESSAGELISTINTERNAL_H_116E8008_B367_4F5C_923C_14993AD2E62E

#include <zmq.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_Exception;

struct B_MessageList {
    size_t const message_count;
    zmq_msg_t messages[1];
};

struct B_MessageList *
b_message_list_allocate(
    void);

struct B_MessageList *
b_message_list_allocate_from_socket(
    void *socket_zmq,
    int flags,
    struct B_Exception **);

void
b_message_list_deallocate(
    struct B_MessageList *);

struct B_MessageList *
b_message_list_append_message_move(
    struct B_MessageList *,
    zmq_msg_t *);

#ifdef __cplusplus
}
#endif

#endif
