#include <B/BuildContext.h>
#include <B/Client.h>
#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/Allocate.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>
#include <B/Question.h>
#include <B/QuestionVTableList.h>
#include <B/Rule.h>
#include <B/RuleQueryList.h>
#include <B/Worker.h>

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>

static struct B_Exception *
b_worker_build(
    struct B_FiberContext *,
    struct B_Client *,
    struct B_AnyRule const *,
    struct B_RuleVTable const *,
    struct B_AnyQuestion const *,
    struct B_QuestionVTable const *);

static struct B_Exception *
b_worker_send_response(
    void *broker_dealer,
    struct B_Identity const *client_identity,
    struct B_RequestID const *request_id,
    struct B_AnyAnswer const *answer,
    struct B_AnswerVTable const *answer_vtable);

static struct B_Exception *
b_worker_recv_request(
    void *socket_zmq,
    struct B_RequestID *,
    struct B_QuestionVTableList const *,
    struct B_Identity **client_identity,
    struct B_AnyQuestion **,
    struct B_QuestionVTable const **);

static struct B_Exception *
b_worker_handle_broker(
    void *broker_dealer,
    struct B_FiberContext *,
    struct B_Client *,
    struct B_AnyRule const *,
    struct B_RuleVTable const *,
    struct B_QuestionVTableList const *);

static struct B_Exception *
b_worker_connect(
    void *context_zmq,
    struct B_Broker const *broker,
    void **out_broker_dealer) {

    struct B_Exception *ex;

    char endpoint_buffer[1024];
    bool ok = b_protocol_worker_endpoint(
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

    b_protocol_send_worker_command(
        broker_dealer,
        B_WORKER_READY,
        0, // flags
        &ex);
    if (ex) {
        return ex;
    }

    *out_broker_dealer = broker_dealer;
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

    struct B_FiberContext *fiber_context;
    ex = b_fiber_context_allocate(&fiber_context);
    if (ex) {
        return ex;
    }

    void *broker_dealer;
    ex = b_worker_connect(
        context_zmq,
        broker,
        &broker_dealer);
    if (ex) {
        // TODO(strager): Cleanup.
        return ex;
    }

    struct B_Client *client;
    ex = b_client_allocate_connect(
        context_zmq,
        broker,
        fiber_context,
        &client);
    if (ex) {
        // TODO(strager): Cleanup.
        return ex;
    }

    zmq_pollitem_t poll_items[] = {
        { broker_dealer, 0, ZMQ_POLLIN, 0 },
    };
    for (;;) {
        const long timeout_milliseconds = -1;
        bool is_finished;
        ex = b_fiber_context_poll_zmq(
            fiber_context,
            poll_items,
            sizeof(poll_items) / sizeof(*poll_items),
            timeout_milliseconds,
            NULL,
            &is_finished);
        if (ex) {
            int errno_value = b_exception_errno_value(ex);
            if (errno_value == ETERM) {
                break;
            }
            if (errno_value == EINTR) {
                continue;
            }
            return ex;
        }
        if (is_finished) {
            break;
        }

        if (poll_items[0].revents & ZMQ_POLLIN) {
            ex = b_worker_handle_broker(
                broker_dealer,
                fiber_context,
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
    (void) zmq_close(broker_dealer);
    (void) b_fiber_context_deallocate(fiber_context);

    return NULL;
}

static struct B_Exception *
b_worker_handle_broker(
    void *broker_dealer,
    struct B_FiberContext *fiber_context,
    struct B_Client *client,
    struct B_AnyRule const *rule,
    struct B_RuleVTable const *rule_vtable,
    struct B_QuestionVTableList const *question_vtables) {

    struct B_Exception *ex;

    // Receive request.
    struct B_Identity *client_identity;
    struct B_RequestID request_id;
    struct B_AnyQuestion *question;
    struct B_QuestionVTable const *question_vtable;
    ex = b_worker_recv_request(
        broker_dealer,
        &request_id,
        question_vtables,
        &client_identity,
        &question,
        &question_vtable);
    if (ex) {
        return ex;
    }

    // Do work.
    ex = b_worker_build(
        fiber_context,
        client,
        rule,
        rule_vtable,
        question,
        question_vtable);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    // Send response.
    struct B_AnyAnswer *answer
        = question_vtable->answer(question, &ex);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    ex = b_worker_send_response(
        broker_dealer,
        client_identity,
        &request_id,
        answer,
        question_vtable->answer_vtable);
    if (ex) {
        // TODO(strager): Clean up.
        return ex;
    }

    return NULL;
}

static struct B_Exception *
b_worker_send_response(
    void *broker_dealer,
    struct B_Identity const *client_identity,
    struct B_RequestID const *request_id,
    struct B_AnyAnswer const *answer,
    struct B_AnswerVTable const *answer_vtable) {

    struct B_Exception *ex = NULL;

    b_protocol_send_worker_command(
        broker_dealer,
        B_WORKER_DONE_AND_READY,
        ZMQ_SNDMORE, // flags
        &ex);
    if (ex) {
        return ex;
    }

    b_protocol_send_identity_envelope(
        broker_dealer,
        client_identity,
        ZMQ_SNDMORE,
        &ex);
    if (ex) {
        return ex;
    }

    ex = b_protocol_send_request_id(
        broker_dealer,
        request_id,
        ZMQ_SNDMORE);
    if (ex) {
        return ex;
    }

    b_protocol_send_answer(
        broker_dealer,
        answer,
        answer_vtable,
        0,  // flags
        &ex);
    if (ex) {
        return ex;
    }

    return NULL;
}

static struct B_Exception *
b_worker_recv_request(
    void *socket_zmq,
    struct B_RequestID *request_id,
    struct B_QuestionVTableList const *question_vtables,
    struct B_Identity **client_identity,
    struct B_AnyQuestion **question,
    struct B_QuestionVTable const **question_vtable) {

    struct B_Exception *ex = NULL;

    ex = b_protocol_recv_identity_delimiter(
        socket_zmq,
        0);  // flags
    if (ex) {
        return ex;
    }

    *client_identity = b_protocol_recv_identity_envelope(
        socket_zmq,
        0,  // flags
        &ex);
    if (ex) {
        return ex;
    }

    ex = b_protocol_recv_request_id(
        socket_zmq,
        request_id,
        0);  // flags
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

struct B_WorkerQueryClosure {
    B_RuleQueryFunction rule_query_function;
    struct B_BuildContext *build_context;
    struct B_AnyQuestion const *question;
    void *closure;
};

// TODO(strager): Better function name.
static void *
b_worker_query(
    void *closure_raw) {

    struct B_Exception *ex = NULL;
    struct B_WorkerQueryClosure *closure = closure_raw;
    closure->rule_query_function(
        closure->build_context,
        closure->question,
        closure->closure,
        &ex);
    return NULL;
}

static struct B_Exception *
b_worker_build(
    struct B_FiberContext *fiber_context,
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

        struct B_RuleQuery const *rule_query
            = b_rule_query_list_get(rule_query_list, 0);

        struct B_WorkerQueryClosure closure = {
            .rule_query_function = rule_query->function,
            .build_context = build_context,
            .question = question,
            .closure = rule_query->closure,
        };
        struct B_Exception *fork_ex
            = b_fiber_context_fork(
                fiber_context,
                b_worker_query,
                &closure,
                (void **) &ex);
        B_EXCEPTION_THEN(&fork_ex, {
            ex = fork_ex;
            goto done;
        });
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
