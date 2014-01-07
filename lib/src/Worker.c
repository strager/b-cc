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

#if defined(B_DEBUG)
#include <B/Internal/Identity.h>
#endif

#include <zmq.h>

#include <assert.h>
#include <stdbool.h>

struct B_Worker {
    void *context_zmq;
    struct B_BrokerAddress const *const broker_address;
    struct B_FiberContext *const fiber_context;

    struct B_QuestionVTableList const *const question_vtables;
    struct B_AnyRule const *const rule;
    struct B_RuleVTable const *const rule_vtable;

};

struct B_WorkerFiberClosure {
    struct B_Worker const *const worker;
    bool volatile should_die;
    bool volatile did_die;
};

static B_ERRFUNC
b_worker_work_fiber(
    struct B_Worker const *,
    bool const volatile *should_die);

static B_ERRFUNC
b_worker_exit_abandon(
    void *broker_dealer,
    struct B_FiberContext *);

static void *
b_worker_work_fiber_wrapper(
    void *);

static struct B_Exception *
b_worker_build(
    struct B_Worker const *,
    struct B_Client *,
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
    struct B_Client *,
    struct B_Worker const *);

static struct B_Exception *
b_worker_handle_broker_request_response(
    void *broker_dealer,
    struct B_Client *,
    struct B_Worker const *);

static struct B_Exception *
b_worker_connect(
    void *context_zmq,
    struct B_FiberContext *fiber_context,
    struct B_BrokerAddress const *broker_address,
    void **out_broker_dealer) {

    struct B_Exception *ex;

    void *broker_dealer;
    ex = b_zmq_socket(
        context_zmq,
        ZMQ_DEALER,
        &broker_dealer);
    if (ex) {
        return ex;
    }

#if defined(B_DEBUG)
    {
        void *fiber_id;
        ex = b_fiber_context_current_fiber_id(
            fiber_context,
            &fiber_id);
        if (ex) {
            goto failed_to_get_id;
        }

        B_LOG(B_INFO, "fiber_id=%p", fiber_id);
        struct B_Identity *identity
            = b_identity_allocate_fiber_id(
                context_zmq,
                NULL,
                fiber_id);
        assert(identity);

        ex = b_identity_set_for_socket(
            broker_dealer,
            identity);
        b_identity_deallocate(identity);
        if (ex) {
            return ex;
        }
failed_to_get_id: (void) 0;
    }
#endif

    ex = b_protocol_connectbind_worker(
        broker_dealer,
        broker_address,
        B_CONNECT);
    if (ex) {
        (void) b_zmq_close(broker_dealer);
        return ex;
    }

    *out_broker_dealer = broker_dealer;
    return NULL;
}

B_ERRFUNC
b_worker_work(
    void *context_zmq,
    struct B_BrokerAddress const *broker_address,
    struct B_QuestionVTableList const *question_vtables,
    struct B_AnyRule const *rule,
    struct B_RuleVTable const *rule_vtable) {

    struct B_Exception *ex;

    struct B_FiberContext *fiber_context;
    ex = b_fiber_context_allocate(&fiber_context);
    if (ex) {
        return ex;
    }

    struct B_Worker worker = {
        .context_zmq = context_zmq,
        .broker_address = broker_address,
        .fiber_context = fiber_context,

        .question_vtables = question_vtables,
        .rule = rule,
        .rule_vtable = rule_vtable,
    };

    bool volatile const should_die = false;
    ex = b_worker_work_fiber(&worker, &should_die);
    if (ex) {
        // TODO(strager): Cleanup.
        return ex;
    }

    // TODO(strager): Error checking.
    (void) b_fiber_context_deallocate(fiber_context);

    return NULL;
}


static B_ERRFUNC
b_worker_work_fiber_with_socket(
    void *broker_dealer,
    struct B_Client *client,
    struct B_Worker const *worker,
    bool const volatile *should_die) {

#if 0
    if (*should_die) {
        return NULL;
    }
#endif

    {
        struct B_Exception *ex = NULL;
        b_protocol_send_worker_command(
            broker_dealer,
            B_WORKER_READY,
            0, // flags
            &ex);
        if (ex) {
            // TODO(strager): Cleanup.
            return ex;
        }
    }

    zmq_pollitem_t poll_items[] = {
        { broker_dealer, 0, ZMQ_POLLIN, 0 },
    };
    while (!*should_die) {
        const long timeout_milliseconds = -1;
        bool is_finished;
        struct B_Exception *ex = b_fiber_context_poll_zmq(
            worker->fiber_context,
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
                client,
                worker);
            if (ex) {
                // FIXME(strager): Should we tell the broker
                // we are done?
                return ex;
            }
        }
    }

    // Tell the broker we are done.
    {
        struct B_Exception *ex = b_worker_exit_abandon(
            broker_dealer,
            worker->fiber_context);
        if (ex) {
            return ex;
        }
    }

    return NULL;
}

static B_ERRFUNC
b_worker_work_fiber(
    struct B_Worker const *worker,
    bool const volatile *should_die) {

    struct B_Exception *ex;

#if 0
    if (*should_die) {
        return NULL;
    }
#endif

    B_LOG_FIBER(
        B_INFO,
        worker->fiber_context,
        "Worker fiber started.");

    struct B_Client *client;
    ex = b_client_allocate_connect(
        worker->context_zmq,
        worker->broker_address,
        worker->fiber_context,
        &client);
    if (ex) {
        return ex;
    }

    void *broker_dealer;
    ex = b_worker_connect(
        worker->context_zmq,
        worker->fiber_context,
        worker->broker_address,
        &broker_dealer);
    if (ex) {
        // TODO(strager): Cleanup.
        return ex;
    }

    ex = b_worker_work_fiber_with_socket(
        broker_dealer,
        client,
        worker,
        should_die);
    if (ex) {
        (void) b_zmq_close(broker_dealer);
        return ex;
    }

    ex = b_zmq_close(broker_dealer);
    if (ex) {
        // TODO(strager): Cleanup.
        return ex;
    }

    // TODO(strager): Error handling.
    (void) b_client_deallocate_disconnect(client);

    return NULL;
}

static B_ERRFUNC
b_worker_exit_abandon(
    void *broker_dealer,
    struct B_FiberContext *fiber_context) {

    B_LOG_FIBER(B_INFO, fiber_context, "Abandoning work.");

    {
        struct B_Exception *ex = NULL;
        b_protocol_send_worker_command(
            broker_dealer,
            B_WORKER_EXIT,
            0, // flags
            &ex);
        if (ex) {
            return ex;
        }
    }

    // Possible response orderings:
    // * ready, then exit, then abandon
    // * exit only
    bool expecting_ready_response = true;
    bool expecting_exit_response = true;
    bool expecting_abandon_response = false;

    zmq_pollitem_t poll_items[] = {
        { broker_dealer, 0, ZMQ_POLLIN, 0 },
    };
    for (;;) {
        // FIXME(strager): We should probably have a
        // reasonable timeout (1 second?).
        B_LOG_FIBER(B_INFO, fiber_context, "hello");
        const long timeout_milliseconds = -1;
        bool is_finished;
        struct B_Exception *ex = b_fiber_context_poll_zmq(
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

        // Received response from broker.
        // TODO(strager): Factor into some function
        // goodness.
        if (poll_items[0].revents & ZMQ_POLLIN) {
            ex = b_protocol_recv_identity_delimiter(
                broker_dealer,
                0);  // flags
            if (ex) {
                return ex;
            }

            zmq_msg_t first_message;
            ex = b_zmq_msg_init_recv(
                &first_message,
                broker_dealer,
                0);  // flags
            if (ex) {
                return ex;
            }

            if (zmq_msg_size(&first_message) == 0) {
                if (zmq_msg_more(&first_message)) {
                    return b_exception_string(
                        "Expected non-empty message or final empty message");
                }

                // Broker has confirmed our WORKER_EXIT or
                // WORKER_ABANDON request.
                b_zmq_msg_close(&first_message);

                if (expecting_exit_response) {
                    B_LOG_FIBER(
                        B_INFO,
                        fiber_context,
                        "WORKER_EXIT response received.");
                    expecting_exit_response = false;
                }
                if (expecting_abandon_response) {
                    B_LOG_FIBER(
                        B_INFO,
                        fiber_context,
                        "WORKER_ABANDON response received.");
                    expecting_abandon_response = false;
                }

                if (!expecting_exit_response
                    && !expecting_abandon_response) {
                    break;
                }
            } else {
                // Broker is sending us work.  Send it back.

                if (!expecting_ready_response) {
                    b_zmq_msg_close(&first_message);
                    return b_exception_string(
                        "Received multiple work responses");
                }
                expecting_ready_response = false;

                B_LOG_FIBER(
                    B_INFO,
                    fiber_context,
                    "Received work load; sending back as abandoned.");
                b_protocol_send_worker_command(
                    broker_dealer,
                    B_WORKER_ABANDON,
                    ZMQ_SNDMORE,
                    &ex);
                if (ex) {
                    b_zmq_msg_close(&first_message);
                    return ex;
                }

                ex = b_zmq_msg_resend(
                    &first_message,
                    broker_dealer,
                    0);  // flags
                b_zmq_msg_close(&first_message);
                if (ex) {
                    return ex;
                }

                expecting_abandon_response = true;
            }
        }
    }

    return NULL;
}

static void *
b_worker_work_fiber_wrapper(
    void *closure_raw) {

    struct B_WorkerFiberClosure *closure = closure_raw;
    struct B_Exception *ex = b_worker_work_fiber(
        closure->worker,
        &closure->should_die);
    closure->did_die = true;
    B_LOG_FIBER(
        B_INFO,
        closure->worker->fiber_context,
        "Worker fiber did die.");
    if (ex) {
        return ex;
    }

    return NULL;
}

static struct B_Exception *
b_worker_handle_broker(
    void *broker_dealer,
    struct B_Client *client,
    struct B_Worker const *worker) {

    // Fork off a fiber which will do work while a Client
    // waits for a response from other workers.
    B_LOG_FIBER(
        B_INFO,
        worker->fiber_context,
        "Forking temporary worker fiber.");
    struct B_WorkerFiberClosure fiber_closure = {
        .worker = worker,
        .should_die = false,
        .did_die = false,
    };
    struct B_Exception *fiber_ex;
    struct B_Exception *ex = b_fiber_context_fork(
        worker->fiber_context,
        b_worker_work_fiber_wrapper,
        &fiber_closure,
        (void *) &fiber_ex);
    if (ex) {
        return ex;
    }

    struct B_Exception *handle_ex
        = b_worker_handle_broker_request_response(
            broker_dealer,
            client,
            worker);

    // Kill fiber and wait for it to die.
    B_LOG_FIBER(
        B_INFO,
        worker->fiber_context,
        "Killing forked worker.");
    fiber_closure.should_die = true;
    while (!fiber_closure.did_die) {
        struct B_Exception *yield_ex
            = b_fiber_context_hard_yield(
                worker->fiber_context);
        if (yield_ex) {
            if (handle_ex) {
                return handle_ex;
            }
            return yield_ex;
        }
    }

    if (handle_ex) {
        return handle_ex;
    }

    if (fiber_ex) {
        // FIXME(strager): This shouldn't terminate this
        // fiber.
        return fiber_ex;
    }

    return NULL;
}

static struct B_Exception *
b_worker_handle_broker_request_response(
    void *broker_dealer,
    struct B_Client *client,
    struct B_Worker const *worker) {

    struct B_Exception *ex;

    // Receive request.
    struct B_Identity *client_identity;
    struct B_RequestID request_id;
    struct B_AnyQuestion *question;
    struct B_QuestionVTable const *question_vtable;
    ex = b_worker_recv_request(
        broker_dealer,
        &request_id,
        worker->question_vtables,
        &client_identity,
        &question,
        &question_vtable);
    if (ex) {
        return ex;
    }

    // Do work.
    ex = b_worker_build(
        worker,
        client,
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
        ZMQ_SNDMORE,
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

static struct B_Exception *
b_worker_build(
    struct B_Worker const *worker,
    struct B_Client *client,
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
    worker->rule_vtable->query(
        worker->rule,
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
        B_LOG_FIBER(
            B_INFO,
            worker->fiber_context,
            "Executing rule.");

        struct B_RuleQuery const *rule_query
            = b_rule_query_list_get(rule_query_list, 0);
        rule_query->function(
            build_context,
            question,
            rule_query->closure,
            &ex);
        B_EXCEPTION_THEN(&ex, {
            goto done;
        });

        B_LOG_FIBER(
            B_INFO,
            worker->fiber_context,
            "Done executing rule.");
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
