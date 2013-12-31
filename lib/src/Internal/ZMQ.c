#include <B/Exception.h>
#include <B/Internal/Portable.h>
#include <B/Internal/Util.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>

#include <zmq.h>

#include <assert.h>

static const size_t
log_bytes_per_line = 16;

B_ERRFUNC
b_zmq_socket_connect(
    void *context_zmq,
    int socket_type,
    char const *endpoint,
    void **out_socket_zmq) {

    void *socket_zmq = zmq_socket(context_zmq, socket_type);
    if (!socket_zmq) {
        return b_exception_errno("zmq_socket", errno);
    }

    int rc = zmq_connect(socket_zmq, endpoint);
    if (rc == -1) {
        return b_exception_errno("zmq_connect", errno);
    }

    B_LOG(B_ZMQ, "Socket %p created and connected.", socket_zmq);

    *out_socket_zmq = socket_zmq;
    return NULL;
}

B_ERRFUNC
b_zmq_socket_bind(
    void *context_zmq,
    int socket_type,
    char const *endpoint,
    void **out_socket_zmq) {

    void *socket_zmq = zmq_socket(context_zmq, socket_type);
    if (!socket_zmq) {
        return b_exception_errno("zmq_socket", errno);
    }

    int rc = zmq_bind(socket_zmq, endpoint);
    if (rc == -1) {
        return b_exception_errno("zmq_bind", errno);
    }

    B_LOG(B_ZMQ, "Socket %p created and bound.", socket_zmq);

    *out_socket_zmq = socket_zmq;
    return NULL;
}

B_ERRFUNC
b_zmq_close(
    void *socket_zmq) {

    B_LOG(B_ZMQ, "Closing socket %p.", socket_zmq);

    int rc = zmq_close(socket_zmq);
    if (rc == -1) {
        return b_exception_errno("zmq_close", errno);
    }

    return NULL;
}

static void
b_log_msg_zmq(
    zmq_msg_t *message);

static void
b_log_msg_data(
    void const *data,
    size_t data_size);

static struct B_Exception *
b_log_zmq_line(
    char const *line,
    void *user_closure);

void
b_zmq_msg_init(
    zmq_msg_t *message) {

    int rc = zmq_msg_init(message);
    assert(rc == 0);
}

void
b_zmq_msg_close(
    zmq_msg_t *message) {

    int rc = zmq_msg_close(message);
    assert(rc == 0);
}

struct B_Exception *
b_zmq_msg_init_recv(
    zmq_msg_t *message,
    void *socket_zmq,
    int flags) {

    b_zmq_msg_init(message);

    struct B_Exception *ex
        = b_zmq_msg_recv(message, socket_zmq, flags);
    if (ex) {
        b_zmq_msg_close(message);
        return ex;
    }

    return NULL;
}

struct B_Exception *
b_zmq_msg_recv(
    zmq_msg_t *message,
    void *socket_zmq,
    int flags) {

    int rc;

retry:
    rc = zmq_msg_recv(message, socket_zmq, flags);
    if (rc == -1) {
        if (errno == EINTR) {
            goto retry;
        }
        return b_exception_errno("zmq_msg_recv", errno);
    }

    {
        b_log_lock();
        B_LOG(
            B_ZMQ,
            "b_zmq_msg_recv(%p): Received%s message (%zu bytes):",
            socket_zmq,
            zmq_msg_more(message) ? "" : " final",
            zmq_msg_size(message));
        b_log_msg_zmq(message);
        b_log_unlock();
    }

    return NULL;
}

struct B_Exception *
b_zmq_msg_send(
    zmq_msg_t *message,
    void *socket_zmq,
    int flags) {

    {
        b_log_lock();
        B_LOG(
            B_ZMQ,
            "b_zmq_msg_send(%p): Sending%s message (%zu bytes):",
            socket_zmq,
            flags & ZMQ_SNDMORE ? "" : " final",
            zmq_msg_size(message));
        b_log_msg_zmq(message);
        b_log_unlock();
    }

    int rc = zmq_msg_send(message, socket_zmq, flags);
    if (rc == -1) {
        return b_exception_errno("zmq_msg_send", errno);
    }

    return NULL;
}

struct B_Exception *
b_zmq_msg_resend(
    zmq_msg_t *message,
    void *socket_zmq,
    int flags) {

    int extra_flags
        = zmq_msg_more(message) ? ZMQ_SNDMORE : 0;
    return b_zmq_msg_send(
        message,
        socket_zmq,
        flags | extra_flags);
}

struct B_Exception *
b_zmq_send(
    void *socket_zmq,
    void const *data,
    size_t data_size,
    int flags) {

    int rc = zmq_send(socket_zmq, data, data_size, flags);
    if (rc == -1) {
        return b_exception_errno("zmq_send", errno);
    }
    assert(rc >= 0);
    assert((size_t) rc == data_size);

    {
        b_log_lock();
        B_LOG(
            B_ZMQ,
            "b_zmq_send(%p): Sent%s message (%zu bytes):",
            socket_zmq,
            flags & ZMQ_SNDMORE ? "" : " final",
            data_size);
        b_log_msg_data(data, data_size);
        b_log_unlock();
    }

    return NULL;
}

struct B_Exception *
b_zmq_recv(
    void *socket_zmq,
    void *data,
    size_t *data_size,
    int flags) {

    int rc = zmq_recv(socket_zmq, data, *data_size, flags);
    if (rc == -1) {
        return b_exception_errno("zmq_recv", errno);
    }
    assert(rc >= 0);

    {
        b_log_lock();
        B_LOG(
            B_ZMQ,
            "b_zmq_recv(%p): Received%s message (%zu bytes):",
            socket_zmq,
            b_zmq_socket_more(socket_zmq) ? "" : " final",
            *data_size);
        b_log_msg_data(data, *data_size);
        b_log_unlock();
    }

    *data_size = (size_t) rc;
    return NULL;
}

bool
b_zmq_socket_more(
    void *socket_zmq) {

    int more;
    int rc = zmq_getsockopt(
        socket_zmq,
        ZMQ_RCVMORE,
        &more,
        &(size_t) {sizeof(more)});
    assert(rc == 0);

    return more;
}

B_ERRFUNC
b_zmq_copy(
    void *from_socket_zmq,
    void *to_socket_zmq,
    int flags) {

    bool more_messages = true;
    while (more_messages) {
        zmq_msg_t message;

        struct B_Exception *ex = b_zmq_msg_init_recv(
            &message,
            from_socket_zmq,
            0);  // flags
        if (ex) {
            return ex;
        }

        more_messages = zmq_msg_more(&message);

        int extra_flags = more_messages ? ZMQ_SNDMORE : 0;
        ex = b_zmq_msg_send(
            &message,
            to_socket_zmq,
            flags | extra_flags);
        b_zmq_msg_close(&message);
        if (ex) {
            return ex;
        }
    }

    return NULL;
}

static void
b_log_msg_zmq(
    zmq_msg_t *message) {

    b_log_msg_data(
        zmq_msg_data(message),
        zmq_msg_size(message));
}

static void
b_log_msg_data(
    void const *data,
    size_t data_size) {

    struct B_Exception *ex = b_hexdump(
        data,
        data_size,
        log_bytes_per_line,
        b_log_zmq_line,
        NULL);
    assert(!ex);
}

static struct B_Exception *
b_log_zmq_line(
    char const *line,
    void *user_closure) {

    (void) user_closure;
    B_LOG(B_ZMQ, "%s", line);
    return NULL;
}
