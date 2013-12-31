#include <B/Broker.h>
#include <B/Exception.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Identity.h>
#include <B/Internal/MessageList.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct B_WorkerQueue {
    // Owned.
    struct B_WorkerQueue *const next;

    // Owned.
    struct B_Identity *const worker_identity;
};

struct B_MessageQueue {
    // Owned.
    struct B_MessageQueue *const next;

    // Owned.
    struct B_MessageList *const messages;
};

struct B_Broker {
    struct B_AnyDatabase const *const database;
    struct B_DatabaseVTable const *const database_vtable;

    // ZeroMQ sockets
    void *const client_router;
    void *const worker_router;

    // Invariant:
    // (ready_workers == NULL) != (work_queue == NULL)
    struct B_WorkerQueue *ready_workers;
    struct B_MessageQueue *work_queue;
};

static struct B_Exception *
b_broker_bind_process(
    struct B_Broker const *,
    void *context_zmq,
    void **out_client_router,
    void **out_worker_router);

static struct B_Exception *
b_broker_handle_client(
    struct B_Broker *);

static struct B_Exception *
b_broker_handle_worker(
    struct B_Broker *);

static void
b_ready_workers_deallocate(
    struct B_WorkerQueue *);

static void
b_message_queue_deallocate(
    struct B_MessageQueue *);

struct B_Exception *
b_broker_allocate_bind(
    void *context_zmq,
    struct B_AnyDatabase *database,
    struct B_DatabaseVTable const *database_vtable,
    struct B_Broker **out) {

    // The b_protocol_*_endpoint functions only use the
    // pointer value of a Broker, so it is safe to give them
    // uninitialized memory.
    struct B_Broker *broker
        = malloc(sizeof(struct B_Broker));

    void *client_router;
    void *worker_router;
    {
        struct B_Exception *ex = b_broker_bind_process(
            broker,
            context_zmq,
            &client_router,
            &worker_router);
        if (ex) {
            free(broker);
            return ex;
        }
    }

    *broker = (struct B_Broker) {
        .database = database,
        .database_vtable = database_vtable,

        .client_router = client_router,
        .worker_router = worker_router,
    };

    *out = broker;
    return NULL;
}

struct B_Exception *
b_broker_deallocate_unbind(
    struct B_Broker *broker) {

    int rc;
    struct B_Exception *ex = NULL;

    rc = zmq_close(broker->client_router);
    if (rc == -1 && !ex) {
        ex = b_exception_errno("zmq_close", errno);
    }

    rc = zmq_close(broker->worker_router);
    if (rc == -1 && !ex) {
        ex = b_exception_errno("zmq_close", errno);
    }

    b_ready_workers_deallocate(broker->ready_workers);
    b_message_queue_deallocate(broker->work_queue);

    free(broker);

    return ex;
}

static struct B_Exception *
b_broker_bind_process(
    struct B_Broker const *broker,
    void *context_zmq,
    void **out_client_router,
    void **out_worker_router) {

    bool ok;
    char endpoint_buffer[1024];

    ok = b_protocol_client_endpoint(
        endpoint_buffer,
        sizeof(endpoint_buffer),
        broker);
    assert(ok);

    void *client_router;
    {
        struct B_Exception *ex = b_zmq_socket_bind(
            context_zmq,
            ZMQ_ROUTER,
            endpoint_buffer,
            &client_router);
        if (ex) {
            return ex;
        }
    }

    ok = b_protocol_worker_endpoint(
        endpoint_buffer,
        sizeof(endpoint_buffer),
        broker);
    assert(ok);

    void *worker_router;
    {
        struct B_Exception *ex = b_zmq_socket_bind(
            context_zmq,
            ZMQ_ROUTER,
            endpoint_buffer,
            &worker_router);
        if (ex) {
            (void) b_zmq_close(client_router);
            return ex;
        }
    }

    *out_client_router = client_router;
    *out_worker_router = worker_router;
    return NULL;
}

struct B_Exception *
b_broker_run(
    struct B_Broker *broker) {

    zmq_pollitem_t poll_items[] = {
        { broker->client_router, 0, ZMQ_POLLIN, 0 },
        { broker->worker_router, 0, ZMQ_POLLIN, 0 },
    };

    for (;;) {
        const long timeout = -1;
        int rc = zmq_poll(
            poll_items,
            sizeof(poll_items) / sizeof(*poll_items),
            timeout);
        if (rc == -1) {
            if (errno == ETERM) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == ENOTSOCK) {
                // HACK(strager):
                // b_broker_deallocate_unbind called.
                break;
            }
            return b_exception_errno("zmq_poll", errno);
        }

        if (poll_items[0].revents & ZMQ_POLLIN) {
            struct B_Exception *ex
                = b_broker_handle_client(broker);
            if (ex) {
                return ex;
            }
        }
        if (poll_items[1].revents & ZMQ_POLLIN) {
            struct B_Exception *ex
                = b_broker_handle_worker(broker);
            if (ex) {
                return ex;
            }
        }
    }

    return NULL;
}

static void
b_broker_copy_messages(
    void *from_socket_zmq,
    void *to_socket_zmq,
    struct B_Exception **ex) {

    bool more_messages = true;
    while (more_messages) {
        zmq_msg_t message;

        *ex = b_zmq_msg_init_recv(
            &message,
            from_socket_zmq,
            0);
        if (*ex) {
            return;
        }

        more_messages = zmq_msg_more(&message);

        *ex = b_zmq_msg_send(
            &message,
            to_socket_zmq,
            more_messages ? ZMQ_SNDMORE : 0);
        b_zmq_msg_close(&message);
        if (*ex) {
            return;
        }
    }
}

static struct B_Exception *
b_broker_copy_request_to_worker(
    struct B_Broker *broker,
    struct B_Identity const *worker_identity) {

    struct B_Exception *ex = NULL;
    b_protocol_send_identity_envelope(
        broker->worker_router,
        worker_identity,
        ZMQ_SNDMORE,
        &ex);
    if (ex) {
        return ex;
    }

    b_broker_copy_messages(
        broker->client_router,
        broker->worker_router,
        &ex);
    if (ex) {
        return ex;
    }

    return NULL;
}

static B_ERRFUNC
b_broker_send_request_to_worker(
    struct B_Broker *broker,
    struct B_Identity const *worker_identity,
    struct B_MessageList *message_list) {

    struct B_Exception *ex = NULL;
    b_protocol_send_identity_envelope(
        broker->worker_router,
        worker_identity,
        ZMQ_SNDMORE,
        &ex);
    if (ex) {
        return ex;
    }

    ex = b_message_list_send(
        message_list,
        broker->worker_router,
        0);  // flags
    if (ex) {
        return ex;
    }

    return NULL;
}

static void
b_broker_enqueue_work(
    struct B_Broker *broker,
    struct B_MessageList *message_list) {

    B_ALLOCATE(struct B_MessageQueue, queue_item, {
        .next = broker->work_queue,
        .messages = message_list,
    });
    broker->work_queue = queue_item;
}

static void
b_ready_workers_deallocate(
    struct B_WorkerQueue *worker_queue) {

    while (worker_queue) {
        struct B_WorkerQueue *next = worker_queue->next;

        b_identity_deallocate(
            worker_queue->worker_identity);
        free(worker_queue);

        worker_queue = next;
    }
}

static void
b_message_queue_deallocate(
    struct B_MessageQueue *message_queue) {

    while (message_queue) {
        struct B_MessageQueue *next = message_queue->next;

        b_message_list_deallocate(
            message_queue->messages);
        free(message_queue);

        message_queue = next;
    }
}

static B_ERRFUNC
b_broker_worker_ready(
    struct B_Broker *broker,
    struct B_Identity *identity) {

    if (broker->work_queue) {
        // Work queued; send to worker immediately.
        struct B_MessageQueue work_item
            = *broker->work_queue;
        free(broker->work_queue);
        broker->work_queue = work_item.next;

        struct B_Exception *ex
            = b_broker_send_request_to_worker(
                broker,
                identity,
                work_item.messages);
        b_message_list_deallocate(work_item.messages);

        if (ex) {
            return ex;
        }

        return NULL;
    } else {
        // No work queued; queue the worker.
        B_ALLOCATE(struct B_WorkerQueue, queue_item, {
            .next = broker->ready_workers,
            .worker_identity = identity,
        });
        broker->ready_workers = queue_item;

        return NULL;
    }
}

static struct B_Identity *
b_broker_take_ready_worker(
    struct B_Broker *broker) {

    struct B_WorkerQueue *queue_item_ptr
        = broker->ready_workers;
    assert(queue_item_ptr);

    struct B_WorkerQueue queue_item = *queue_item_ptr;
    free(queue_item_ptr);

    broker->ready_workers = queue_item.next;
    return queue_item.worker_identity;
}

static struct B_Exception *
b_broker_handle_client(
    struct B_Broker *broker) {

    if (broker->ready_workers) {
        B_LOG(B_INFO, "Sending worker work to do.");

        struct B_Identity *worker_identity
            = b_broker_take_ready_worker(broker);
        assert(worker_identity);

        struct B_Exception *ex
            = b_broker_copy_request_to_worker(
                broker,
                worker_identity);
        b_identity_deallocate(worker_identity);
        if (ex) {
            return ex;
        }

        return NULL;
    } else {
        B_LOG(B_INFO, "No workers available; queueing work for next available worker.");

        struct B_Exception *ex = NULL;
        struct B_MessageList *message_list
            = b_message_list_allocate_from_socket(
                broker->client_router,
                0,  // flags
                &ex);
        if (ex) {
            return ex;
        }
        b_broker_enqueue_work(broker, message_list);

        return NULL;
    }
}

static struct B_Exception *
b_broker_handle_worker(
    struct B_Broker *broker) {

    struct B_Exception *ex = NULL;

    struct B_Identity *worker_identity
        = b_protocol_recv_identity_envelope(
            broker->worker_router,
            0,  // flags
            &ex);
    if (ex) {
        return ex;
    }

    enum B_WorkerCommand worker_command
        = b_protocol_recv_worker_command(
            broker->worker_router,
            0,  // flags
            &ex);
    if (ex) {
        b_identity_deallocate(worker_identity);
        return ex;
    }

    switch (worker_command) {
    case B_WORKER_READY:
        B_LOG(B_INFO, "Worker marked as ready.");

        ex = b_broker_worker_ready(broker, worker_identity);
        if (ex) {
            return NULL;
        }

        return NULL;

    case B_WORKER_DONE_AND_READY: {
        B_LOG(B_INFO, "Worker is done and marked as ready.");

        ex = b_broker_worker_ready(broker, worker_identity);
        if (ex) {
            return NULL;
        }

        // Send identity and answer back to client.
        b_broker_copy_messages(
            broker->worker_router,
            broker->client_router,
            &ex);
        if (ex) {
            return ex;
        }

        return NULL;
    }

    default:
        assert(0);  // FIXME(strager)
    }
}
