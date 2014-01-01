#ifndef ZMQ_H_D9C4D3FF_FB0A_46FF_990B_59D6D3A89BC0
#define ZMQ_H_D9C4D3FF_FB0A_46FF_990B_59D6D3A89BC0

#include <B/Internal/Common.h>

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

// Sets ZMQ_SNDMORE if the zmq_msg_more(&message) returns
// nonzero.
B_ERRFUNC
b_zmq_msg_resend(
    zmq_msg_t *message,
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

B_ERRFUNC
b_zmq_copy(
    void *from_socket_zmq,
    void *to_socket_zmq,
    int flags);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
struct B_ZMQContextScope {
    B_ZMQContextScope(
        void *context_zmq) :

        context_zmq(context_zmq) {
    }

    ~B_ZMQContextScope() {
        int rc = zmq_ctx_destroy(this->context_zmq);
        assert(rc == 0);
    }

private:
    void *const context_zmq;
};

struct B_ZMQSocketScope {
    B_ZMQSocketScope(
        void *socket_zmq) :

        socket_zmq(socket_zmq) {
    }

    ~B_ZMQSocketScope() {
        int rc = zmq_close(socket_zmq);
        assert(rc == 0);
    }

private:
    void *const socket_zmq;
};
#endif

#endif
