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
    struct B_WorkerQueue *next;

    // Owned.
    struct B_Identity *const worker_identity;
};

struct B_WorkerList {
    // Owned.
    struct B_WorkerList *next;

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
    void *const context_zmq;

    struct B_AnyDatabase const *const database;
    struct B_DatabaseVTable const *const database_vtable;

    // ZeroMQ sockets
    void *client_router;
    void *worker_router;

    // Invariant:
    // (ready_workers == NULL) != (work_queue == NULL)
    struct B_WorkerQueue *ready_workers;
    struct B_MessageQueue *work_queue;
};

static B_ERRFUNC
b_broker_run_loop(
    struct B_Broker *);

static struct B_Exception *
b_broker_bind_process(
    struct B_Broker const *,
    void *context_zmq,
    void **out_client_router,
    void **out_worker_router);

static struct B_Identity *
b_broker_take_ready_worker(
    struct B_Broker *);

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

    B_ALLOCATE(struct B_Broker, broker, {
        .context_zmq = context_zmq,

        .database = database,
        .database_vtable = database_vtable,

        .client_router = NULL,
        .worker_router = NULL,

        .ready_workers = NULL,
        .work_queue = NULL,
    });

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

    struct B_Exception *ex;

    // The b_protocol_*_endpoint functions only use the
    // pointer value of a Broker, so it is safe to give them
    // an incomplete broker.
    // TODO(strager): Better design.  =]
    ex = b_broker_bind_process(
        broker,
        broker->context_zmq,
        &broker->client_router,
        &broker->worker_router);
    if (ex) {
        return ex;
    }

    ex = b_broker_run_loop(broker);
    // TODO(strager): Capture close errors.
    (void) b_zmq_close(broker->client_router);
    (void) b_zmq_close(broker->worker_router);
    if (ex) {
        return ex;
    }

    return NULL;
}

static B_ERRFUNC
b_broker_run_loop(
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

    *ex = b_zmq_copy(
        from_socket_zmq,
        to_socket_zmq,
        0);  // flags

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
        B_LOG(B_INFO, "Sending worker queued work.");

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
        B_LOG(B_INFO, "No work for worker yet; waiting for work.");

        B_ALLOCATE(struct B_WorkerQueue, queue_item, {
            .next = broker->ready_workers,
            .worker_identity = identity,
        });
        broker->ready_workers = queue_item;

        return NULL;
    }
}

static B_ERRFUNC
b_broker_worker_exit(
    struct B_Broker *broker,
    struct B_Identity *worker_identity) {

    // Remove the worker with 'identity' from the queue.
    struct B_WorkerQueue **prev_next
        = &broker->ready_workers;
    struct B_WorkerQueue *queue_item
        = broker->ready_workers;
    while (queue_item) {
        if (b_identity_equal(
            queue_item->worker_identity,
            worker_identity)) {

            *prev_next = queue_item->next;
            b_identity_deallocate(
                queue_item->worker_identity);
            free(queue_item);

            return NULL;
        }

        prev_next = &queue_item->next;
        queue_item = queue_item->next;
    }

    B_LOG(B_INFO, "Worker which requested exit wasn't found.");

    // Reply.
    {
        struct B_Exception *ex = NULL;
        b_protocol_send_identity_envelope(
            broker->worker_router,
            worker_identity,
            ZMQ_SNDMORE,
            &ex);
        if (ex) {
            return ex;
        }

        ex = b_zmq_send(
            broker->worker_router,
            NULL,
            0,
            0);  // flags
        if (ex) {
            return ex;
        }
    }

    return NULL;
}

static B_ERRFUNC
b_broker_worker_abandon(
    struct B_Broker *broker,
    struct B_Identity const *abandoning_worker_identity) {

    // TODO(strager): Remove duplication with
    // b_broker_handle_client.

    if (broker->ready_workers) {
        B_LOG(B_INFO, "Sending worker work to do.");

        struct B_Identity *worker_identity
            = b_broker_take_ready_worker(broker);
        assert(worker_identity);

        struct B_Exception *ex = NULL;
        b_protocol_send_identity_envelope(
            broker->worker_router,
            worker_identity,
            ZMQ_SNDMORE,
            &ex);
        if (ex) {
            return ex;
        }

        // NOTE(strager): This looks weird, be are routing
        // from one worker to another.
        b_broker_copy_messages(
            broker->worker_router,
            broker->worker_router,
            &ex);
        if (ex) {
            return ex;
        }
    } else {
        B_LOG(B_INFO, "No workers available; queueing work for next available worker.");

        struct B_Exception *ex = NULL;
        struct B_MessageList *message_list
            = b_message_list_allocate_from_socket(
                broker->worker_router,
                0,  // flags
                &ex);
        if (ex) {
            return ex;
        }
        b_broker_enqueue_work(broker, message_list);
    }

    // Reply.
    {
        struct B_Exception *ex = NULL;
        b_protocol_send_identity_envelope(
            broker->worker_router,
            abandoning_worker_identity,
            ZMQ_SNDMORE,
            &ex);
        if (ex) {
            return ex;
        }

        ex = b_zmq_send(
            broker->worker_router,
            NULL,
            0,
            0);  // flags
        if (ex) {
            return ex;
        }
    }

    return NULL;
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

    B_LOG(B_INFO, "b_broker_handle_worker");

    // TODO(strager): Remove duplication with
    // b_broker_worker_abandon.

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

    B_LOG(B_INFO, "b_broker_handle_worker");

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
            return ex;
        }

        return NULL;

    case B_WORKER_DONE_AND_READY: {
        B_LOG(B_INFO, "Worker is done and marked as ready.");

        ex = b_broker_worker_ready(broker, worker_identity);
        if (ex) {
            return ex;
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

    case B_WORKER_DONE_AND_EXIT:
        B_LOG(B_INFO, "Worker is done and exiting.");

        ex = b_broker_worker_exit(broker, worker_identity);
        if (ex) {
            return ex;
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

    case B_WORKER_EXIT:
        B_LOG(B_INFO, "Worker is exiting.");

        ex = b_broker_worker_exit(broker, worker_identity);
        if (ex) {
            return ex;
        }

        return NULL;

    case B_WORKER_ABANDON:
        B_LOG(B_INFO, "Worker is abandoning work.");

        ex = b_broker_worker_abandon(
            broker,
            worker_identity);
        if (ex) {
            return ex;
        }

        return NULL;

    default:
        assert(0);  // FIXME(strager)
    }
}
