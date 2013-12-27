#ifndef ZMQ_H_D9C4D3FF_FB0A_46FF_990B_59D6D3A89BC0
#define ZMQ_H_D9C4D3FF_FB0A_46FF_990B_59D6D3A89BC0

#include "Common.h"

#include <zmq.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Wrappers for ZeroMQ functions.  Provides error reporting
// via B_Exception and optionally adds logging
// functionality.

B_ERRFUNC
b_zmq_socket_connect(
    void *context_zmq,
    int socket_type,  // ZMQ_REQ, ZMQ_SUB, etc.
    char const *endpoint,
    void **out_socket_zmq);

B_ERRFUNC
b_zmq_socket_bind(
    void *context_zmq,
    int socket_type,  // ZMQ_REQ, ZMQ_SUB, etc.
    char const *endpoint,
    void **out_socket_zmq);

B_ERRFUNC
b_zmq_close(
    void *socket_zmq);

void
b_zmq_msg_init(
    zmq_msg_t *);

void
b_zmq_msg_close(
    zmq_msg_t *);

B_ERRFUNC
b_zmq_msg_init_recv(
    zmq_msg_t *,
    void *socket_zmq,
    int flags);

B_ERRFUNC
b_zmq_msg_recv(
    zmq_msg_t *,
    void *socket_zmq,
    int flags);

B_ERRFUNC
b_zmq_msg_send(
    zmq_msg_t *,
    void *socket_zmq,
    int flags);

B_ERRFUNC
b_zmq_send(
    void *socket_zmq,
    void const *data,
    size_t data_size,
    int flags);

B_ERRFUNC
b_zmq_recv(
    void *socket_zmq,
    void *data,
    size_t *data_size,
    int flags);

bool
b_zmq_socket_more(
    void *socket_zmq);

#ifdef __cplusplus
}
#endif

#endif
