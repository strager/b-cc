#include <B/Exception.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Identity.h>
#include <B/Internal/Portable.h>

#include <zmq.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct B_Identity *
b_identity_allocate(
    char const *data,
    size_t data_size) {

    char *data_copy = b_memdup(data, data_size);
    if (!data_copy) {
        return NULL;
    }

    B_ALLOCATE_RETURN(struct B_Identity, {
        .data = data_copy,
        .data_size = data_size,
    });
}

struct B_Identity *
b_identity_allocate_fiber_id(
    void *context_zmq,
    struct B_FiberContext *fiber_context,
    void *fiber_id) {

    // 0xFE [context_zmq] [fiber_context] [fiber_id]
    // Endianness doesn't matter.
    // ZeroMQ reserves 0x00 as the first byte.  We
    // arbitrarily chose 0xFE.
    uint8_t const magic = 0xFE;

    enum {
        data_size
            = sizeof(magic)
                + sizeof(context_zmq)
                + sizeof(fiber_context)
                + sizeof(fiber_id),
    };
    _Static_assert(
        data_size <= 255,
        "Generated identity must not exceed 255 bytes (ZeroMQ limit)");

    uint8_t *data = malloc(data_size);
    if (!data) {
        return NULL;
    }

    uint8_t *p = data;

    memcpy(p, &magic, sizeof(magic));
    p += sizeof(magic);

    memcpy(p, &context_zmq, sizeof(context_zmq));
    p += sizeof(context_zmq);

    memcpy(p, &fiber_context, sizeof(fiber_context));
    p += sizeof(fiber_context);

    memcpy(p, &fiber_id, sizeof(fiber_id));
    p += sizeof(fiber_id);

    assert(p == data + data_size);

    B_ALLOCATE_RETURN(struct B_Identity, {
        .data = (char const *) data,
        .data_size = data_size,
    });
}

void
b_identity_deallocate(
    struct B_Identity *identity) {

    free((char *) identity->data);
    free(identity);
}

bool
b_identity_equal(
    struct B_Identity const *x,
    struct B_Identity const *y) {

    return x->data_size == y->data_size
        && memcmp(x->data, y->data, x->data_size) == 0;
}

B_ERRFUNC
b_identity_set_for_socket(
    void *socket_zmq,
    struct B_Identity const *identity) {

    int rc = zmq_setsockopt(
        socket_zmq,
        ZMQ_IDENTITY,
        identity->data,
        identity->data_size);
    if (rc != 0) {
        return b_exception_errno("zmq_setsockopt", errno);
    }

    return NULL;
}
