#include <B/Exception.h>
#include <B/Internal/MessageList.h>
#include <B/Internal/ZMQ.h>

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#define B_SIZEOF_MESSAGELIST_HEADER \
    (sizeof(struct B_MessageList) - sizeof(((struct B_MessageList *) NULL)->messages))

#define B_SIZEOF_MESSAGELIST(message_count) \
    (B_SIZEOF_MESSAGELIST_HEADER + sizeof(zmq_msg_t) * (message_count))

static struct B_MessageList *
b_message_list_allocate_n(
    size_t message_count) {

    struct B_MessageList *message_list
        = malloc(B_SIZEOF_MESSAGELIST(message_count));
    *(size_t *) &message_list->message_count = message_count;
    return message_list;
}

struct B_MessageList *
b_message_list_allocate(
    void) {

    return b_message_list_allocate_n(0);
}

struct B_MessageList *
b_message_list_allocate_from_socket(
    void *socket_zmq,
    int flags,
    struct B_Exception **ex) {

    struct B_MessageList *message_list
        = b_message_list_allocate();

    bool more_messages = true;
    while (more_messages) {
        int rc;
        zmq_msg_t message;

        rc = zmq_msg_init(&message);
        if (rc == -1) {
            *ex = b_exception_errno("zmq_msg_init", errno);
            return NULL;
        }

        rc = zmq_msg_recv(
            &message,
            socket_zmq,
            flags);
        if (rc == -1) {
            *ex = b_exception_errno("zmq_msg_recv", errno);
            zmq_msg_close(&message);
            return NULL;
        }

        more_messages = zmq_msg_more(&message);

        message_list = b_message_list_append_message_move(
            message_list,
            &message);
    }

    return message_list;
}

void
b_message_list_deallocate(
    struct B_MessageList *message_list) {

    for (size_t i = 0;
        i < message_list->message_count;
        ++i) {
        int rc = zmq_msg_close(&message_list->messages[i]);
        assert(rc == 0);
    }
    free(message_list);
}

struct B_MessageList *
b_message_list_append_message_move(
    struct B_MessageList *message_list,
    zmq_msg_t *message) {

    size_t message_count = message_list->message_count;

    // Move list with one extra message.
    struct B_MessageList *new_message_list
        = b_message_list_allocate_n(message_count + 1);
    for (size_t i = 0; i < message_count; ++i) {
        int rc;
        rc = zmq_msg_init(&new_message_list->messages[i]);
        assert(rc == 0);
        rc = zmq_msg_move(
            &new_message_list->messages[i],
            &message_list->messages[i]);
        assert(rc == 0);
    }
    b_message_list_deallocate(message_list);

    // Move message into extra list entry.
    int rc;
    rc = zmq_msg_init(
        &new_message_list->messages[message_count]);
    assert(rc == 0);
    rc = zmq_msg_move(
        &new_message_list->messages[message_count],
        message);
    assert(rc == 0);

    return new_message_list;
}

B_ERRFUNC
b_message_list_send(
    struct B_MessageList *message_list,
    void *socket_zmq,
    int flags) {

    size_t count = message_list->message_count;
    for (size_t i = 0; i < count; ++i) {
        bool last_message = i == count - 1;
        struct B_Exception *ex = b_zmq_msg_send(
            &message_list->messages[i],
            socket_zmq,
            flags | (last_message ? 0 : ZMQ_SNDMORE));
        if (ex) {
            return ex;
        }
    }

    return NULL;
}
