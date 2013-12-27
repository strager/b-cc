#include "Allocate.h"
#include "Answer.h"
#include "Client.h"
#include "Exception.h"
#include "Log.h"
#include "Protocol.h"
#include "Question.h"
#include "ZMQ.h"

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>

struct B_Client {
    // ZeroMQ sockets
    void *const broker_req;
};

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

    void *broker_req;
    ex = b_zmq_socket_connect(
        context_zmq,
        ZMQ_REQ,
        endpoint_buffer,
        &broker_req);
    if (ex) {
        return ex;
    }

    B_ALLOCATE(struct B_Client, client, {
        .broker_req = broker_req,
    });
    *out = client;

    return NULL;
}

struct B_Exception *
b_client_deallocate_disconnect(
    struct B_Client *client) {

    int rc = zmq_close(client->broker_req);
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

    // TODO(strager): Parallelize.
    for (size_t i = 0; i < count; ++i) {
        struct B_Exception *ex;

        b_protocol_send_uuid(
            client->broker_req,
            question_vtables[i]->uuid,
            ZMQ_SNDMORE,
            &ex);
        if (ex) {
            return ex;
        }

        B_LOG(B_INFO, "Sending question to broker.");
        b_protocol_send_question(
            client->broker_req,
            questions[i],
            question_vtables[i],
            0,  // flags
            &ex);
        if (ex) {
            return ex;
        }

        B_LOG(B_INFO, "Receiving answer from broker.");
        answers[i] = b_protocol_recv_answer(
            client->broker_req,
            question_vtables[i]->answer_vtable,
            0,  // flags
            &ex);
        if (ex) {
            return ex;
        }

        B_LOG(B_INFO, "Ding!");
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
    struct B_Exception *ex = b_client_need_answers(
        client,
        questions,
        question_vtables,
        answers,
        count);
    for (size_t i = 0; i < count; ++i) {
        question_vtables[i]->answer_vtable
            ->deallocate(answers[i]);
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
