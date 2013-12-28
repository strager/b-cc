#include <B/Answer.h>
#include <B/Client.h>
#include <B/Exception.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>
#include <B/Question.h>

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

struct B_Client {
    // ZeroMQ sockets
    void *const broker_dealer;
};

static struct B_Exception *
b_client_send_request(
    void *broker_dealer,
    size_t request_index,
    struct B_AnyQuestion const *,
    struct B_QuestionVTable const *);

static void
serialize_request_index(
    size_t request_index,
    struct B_RequestID *out_request_id);

static size_t
deserialize_request_index(
    struct B_RequestID const *request_id);

struct B_Exception *
b_client_allocate_connect(
    void *context_zmq,
    struct B_Broker const *broker,
    struct B_Client **out) {

    struct B_Exception *ex;

    char endpoint_buffer[1024];
    bool ok = b_protocol_client_endpoint(
        endpoint_buffer,
        sizeof(endpoint_buffer),
        broker);
    assert(ok);

    void *broker_dealer;
    ex = b_zmq_socket_connect(
        context_zmq,
        ZMQ_DEALER,
        endpoint_buffer,
        &broker_dealer);
    if (ex) {
        return ex;
    }

    B_ALLOCATE(struct B_Client, client, {
        .broker_dealer = broker_dealer,
    });
    *out = client;

    return NULL;
}

struct B_Exception *
b_client_deallocate_disconnect(
    struct B_Client *client) {

    int rc = zmq_close(client->broker_dealer);
    if (rc == -1) {
        return b_exception_errno("zmq_close", errno);
    }

    free(client);

    return NULL;
}

struct B_Exception *
b_client_need_answers(
    struct B_Client *client,
    struct B_AnyQuestion const *const *questions,
    struct B_QuestionVTable const *const *question_vtables,
    struct B_AnyAnswer **answers,
    size_t count) {

    for (size_t i = 0; i < count; ++i) {
        struct B_Exception *ex;

        B_LOG(B_INFO, "Sending request %zu to broker.", i);
        ex = b_client_send_request(
            client->broker_dealer,
            i,
            questions[i],
            question_vtables[i]);
        if (ex) {
            return ex;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        answers[i] = NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        struct B_Exception *ex;

        ex = b_protocol_recv_identity_delimiter(
            client->broker_dealer,
            0);  // flags
        if (ex) {
            return ex;
        }

        struct B_RequestID request_id;
        ex = b_protocol_recv_request_id(
            client->broker_dealer,
            &request_id,
            0);  // flags
        if (ex) {
            // TODO(strager): Cleanup.
            return ex;
        }

        size_t request_index
            = deserialize_request_index(&request_id);

        if (request_index >= count) {
            // TODO(strager): Cleanup.
            return b_exception_format_string(
                "Request %zu out of range",
                request_index);
        }
        if (answers[request_index]) {
            // TODO(strager): Cleanup.
            return b_exception_format_string(
                "Request %zu already received",
                request_index);
        }

        answers[request_index] = b_protocol_recv_answer(
            client->broker_dealer,
            question_vtables[request_index]->answer_vtable,
            0,  // flags
            &ex);
        if (ex) {
            return ex;
        }

        B_LOG(B_INFO, "Received reply %zu from broker.", request_index);
    }

    return NULL;
}

struct B_Exception *
b_client_need(
    struct B_Client *client,
    struct B_AnyQuestion const *const *questions,
    struct B_QuestionVTable const *const *question_vtables,
    size_t count) {

    // TODO(strager): Don't allocate/deallocate answers.
    struct B_AnyAnswer *answers[count];
    for (size_t i = 0; i < count; ++i) {
        answers[i] = NULL;
    }

    struct B_Exception *ex = b_client_need_answers(
        client,
        questions,
        question_vtables,
        answers,
        count);
    for (size_t i = 0; i < count; ++i) {
        if (answers[i]) {
            question_vtables[i]->answer_vtable
                ->deallocate(answers[i]);
        }
    }
    if (ex) {
        return ex;
    }

    return NULL;
}

struct B_Exception *
b_client_need_one(
    struct B_Client *client,
    struct B_AnyQuestion const *question,
    struct B_QuestionVTable const *question_vtable) {

    return b_client_need(
        client,
        &question,
        &question_vtable,
        1);  // count
}

static struct B_Exception *
b_client_send_request(
    void *broker_dealer,
    size_t request_index,
    struct B_AnyQuestion const *question,
    struct B_QuestionVTable const *question_vtable) {

    struct B_Exception *ex;

    ex = b_protocol_send_identity_delimiter(
        broker_dealer,
        ZMQ_SNDMORE);
    if (ex) {
        return ex;
    }

    struct B_RequestID request_id;
    serialize_request_index(request_index, &request_id);
    ex = b_protocol_send_request_id(
        broker_dealer,
        &request_id,
        ZMQ_SNDMORE);
    if (ex) {
        return ex;
    }

    b_protocol_send_uuid(
        broker_dealer,
        question_vtable->uuid,
        ZMQ_SNDMORE,
        &ex);
    if (ex) {
        return ex;
    }

    b_protocol_send_question(
        broker_dealer,
        question,
        question_vtable,
        0,  // flags
        &ex);
    if (ex) {
        return ex;
    }

    return NULL;
}

static void
serialize_request_index(
    size_t request_index,
    struct B_RequestID *out_request_id) {

    *out_request_id = (struct B_RequestID) {
        .bytes = {
            request_index >> 0,
            request_index >> 8,
            request_index >> 16,
            request_index >> 24,
        },
    };
}

static size_t
deserialize_request_index(
    struct B_RequestID const *request_id) {

    return (request_id->bytes[0] << 0)
        | (request_id->bytes[1] << 8)
        | (request_id->bytes[2] << 16)
        | (request_id->bytes[3] << 24);
}
