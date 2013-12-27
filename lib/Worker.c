#include "Allocate.h"
#include "BuildContext.h"
#include "Client.h"
#include "Exception.h"
#include "Log.h"
#include "Protocol.h"
#include "Question.h"
#include "QuestionVTableList.h"
#include "Rule.h"
#include "RuleQueryList.h"
#include "Worker.h"
#include "ZMQ.h"

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>

static struct B_Exception *
b_worker_build(
    struct B_Client *,
    struct B_AnyRule const *,
    struct B_RuleVTable const *,
    struct B_AnyQuestion const *,
    struct B_QuestionVTable const *);

static struct B_Exception *
b_worker_parse_request(
    void *socket_zmq,
    struct B_QuestionVTableList const *,
    struct B_Identity **client_identity,
    struct B_AnyQuestion **,
    struct B_QuestionVTable const **);

static struct B_Exception *
b_worker_handle_broker(
    void *broker_req,
    struct B_Client *,
    struct B_AnyRule const *,
    struct B_RuleVTable const *,
    struct B_QuestionVTableList const *);

static struct B_Exception *
b_worker_connect(
    void *context_zmq,
    struct B_Broker const *broker,
    void **out_broker_req) {

    struct B_Exception *ex;

    char endpoint_buffer[1024];
    bool ok = b_protocol_worker_endpoint(
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

    b_protocol_send_worker_command(
        broker_req,
        B_WORKER_READY,
        0, // flags
        &ex);
    if (ex) {
        return ex;
    }

    *out_broker_req = broker_req;
    return NULL;
}

struct B_Exception *
b_worker_work(
    void *context_zmq,
    struct B_Broker const *broker,
    struct B_QuestionVTableList const *question_vtables,
    struct B_AnyRule const *rule,
    struct B_RuleVTable const *rule_vtable) {

    struct B_Exception *ex;

    void *broker_req;
    ex = b_worker_connect(context_zmq, broker, &broker_req);
    if (ex) {
        return ex;
    }

    struct B_Client *client;
    ex = b_client_allocate_connect(
        context_zmq,
        broker,
        &client);
    if (ex) {
        return ex;
    }

    zmq_pollitem_t poll_items[] = {
        { broker_req, 0, ZMQ_POLLIN, 0 },
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
            return b_exception_errno("zmq_poll", errno);
        }

        if (poll_items[0].revents & ZMQ_POLLIN) {
            ex = b_worker_handle_broker(
                broker_req,
                client,
                rule,
                rule_vtable,
                question_vtables);
            if (ex) {
                return ex;
            }
        }
    }

    // TODO(strager): Error checking.
    (void) b_client_deallocate_disconnect(client);
    (void) zmq_close(broker_req);

    return NULL;
}

static struct B_Exception *
b_worker_handle_broker(
    void *broker_req,
    struct B_Client *client,
    struct B_AnyRule const *rule,
    struct B_RuleVTable const *rule_vtable,
    struct B_QuestionVTableList const *question_vtables) {

    struct B_Exception *ex;

    // Receive request.
    struct B_Identity *client_identity;
    struct B_AnyQuestion *question;
    struct B_QuestionVTable const *question_vtable;
    ex = b_worker_parse_request(
        broker_req,
        question_vtables,
        &client_identity,
        &question,
        &question_vtable);
    if (ex) {
        return ex;
    }

    // Do work.
    ex = b_worker_build(
        client,
        rule,
        rule_vtable,
        question,
        question_vtable);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    // Send reply.
    struct B_AnyAnswer *answer
        = question_vtable->answer(question, &ex);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    b_protocol_send_worker_command(
        broker_req,
        B_WORKER_DONE_AND_READY,
        ZMQ_SNDMORE, // flags
        &ex);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    b_protocol_send_identity_envelope(
        broker_req,
        client_identity,
        ZMQ_SNDMORE,
        &ex);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    b_protocol_send_answer(
        broker_req,
        answer,
        question_vtable->answer_vtable,
        0,  // flags
        &ex);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    return NULL;
}

static struct B_Exception *
b_worker_parse_request(
    void *socket_zmq,
    struct B_QuestionVTableList const *question_vtables,
    struct B_Identity **client_identity,
    struct B_AnyQuestion **question,
    struct B_QuestionVTable const **question_vtable) {

    struct B_Exception *ex = NULL;

    *client_identity = b_protocol_recv_identity_envelope(
        socket_zmq,
        0,  // flags
        &ex);
    if (ex) {
        return ex;
    }

    struct B_UUID question_uuid = b_protocol_recv_uuid(
        socket_zmq,
        0,  // flags
        &ex);
    if (ex) {
        return ex;
    }

    *question_vtable = b_question_vtable_list_find_by_uuid(
        question_vtables,
        question_uuid);
    if (!*question_vtable) {
        return b_exception_string(
            "Could not find question vtable from UUID");
    }

    *question = b_protocol_recv_question(
        socket_zmq,
        *question_vtable,
        0,  // flags
        &ex);
    if (ex) {
        return ex;
    }

    return NULL;
}

static struct B_Exception *
b_worker_build(
    struct B_Client *client,
    struct B_AnyRule const *rule,
    struct B_RuleVTable const *rule_vtable,
    struct B_AnyQuestion const *question,
    struct B_QuestionVTable const *question_vtable) {

    // TODO(strager): Database stuff.  Should that be done
    // in the broker?

    struct B_Exception *ex = NULL;

    b_question_validate(question);

    struct B_BuildContext *build_context
        = b_build_context_allocate(client);

    struct B_RuleQueryList *rule_query_list
        = b_rule_query_list_allocate();
    rule_vtable->query(
        rule,
        question,
        question_vtable,
        build_context,
        rule_query_list,
        &ex);
    B_EXCEPTION_THEN(&ex, {
        goto done;
    });

    switch (b_rule_query_list_size(rule_query_list)) {
    case 0:
        ex = b_exception_string("No rule to build question");
        goto done;

    case 1: {
        B_LOG(B_INFO, "Executing rule.");

        const struct B_RuleQuery *rule_query
            = b_rule_query_list_get(rule_query_list, 0);
        rule_query->function(
            build_context,
            question,
            rule_query->closure,
            &ex);
        B_EXCEPTION_THEN(&ex, {
            goto done;
        });

        B_LOG(B_INFO, "Done executing rule.");
        break;
    }

    default:
        ex = b_exception_string("Multiple rules to build question");
        goto done;
    }

done:
    b_rule_query_list_deallocate(rule_query_list);

    return ex;
}
