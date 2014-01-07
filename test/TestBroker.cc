#include <B/Broker.h>
#include <B/Exception.h>
#include <B/Internal/GTest.h>
#include <B/Internal/Portable.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>
#include <B/UUID.h>

#include <gtest/gtest.h>
#include <zmq.h>

static B_ERRFUNC
create_broker_thread(
    void *context_zmq,
    B_BrokerAddress **out_broker_address) {

    B_Exception *ex;

    B_BrokerAddress *broker_address;
    ex = b_broker_address_allocate(
        &broker_address);
    if (ex) {
        return ex;
    }

    ex = b_create_thread(
        "broker",
        [context_zmq, broker_address]() {
            B_Exception *ex = b_broker_run(
                broker_address,
                context_zmq,
                NULL,  // TODO(strager)
                NULL);  // TODO(strager)
            EXPECT_EQ(nullptr, ex);
            if (ex) {
                B_LOG_EXCEPTION(ex);
            }
        });
    if (ex) {
        (void) b_broker_address_deallocate(broker_address);
        return ex;
    }

    *out_broker_address = broker_address;
    return NULL;
}

TEST(TestBroker, WorkBeforeWorker) {
    B_Exception *ex = NULL;

    static B_UUID const question_uuid
        = B_UUID("F5114112-F33B-4A48-882C-CB1E5393A471");
    static uint8_t const question_payload[] = "hello world";
    static uint8_t const answer_payload[] = "g'day to you";

    void *context_zmq = zmq_ctx_new();
    ASSERT_NE(nullptr, context_zmq);
    B_ZMQContextScope context_zmq_scope(context_zmq);

    B_BrokerAddress *broker_address;
    B_CHECK_EX(create_broker_thread(
        context_zmq,
        &broker_address));

    // Send client request.
    void *client_broker_dealer;
    B_CHECK_EX(b_zmq_socket(
        context_zmq,
        ZMQ_DEALER,
        &client_broker_dealer));
    B_ZMQSocketScope client_broker_dealer_scope(
        client_broker_dealer);
    B_CHECK_EX(b_protocol_connectbind_client(
        client_broker_dealer,
        broker_address,
        B_CONNECT));

    B_CHECK_EX(b_protocol_send_identity_delimiter(
        client_broker_dealer,
        ZMQ_SNDMORE));

    {
        B_RequestID request_id = {{0, 1, 2, 3}};
        B_CHECK_EX(b_protocol_send_request_id(
            client_broker_dealer,
            &request_id,
            ZMQ_SNDMORE));
    }

    b_protocol_send_uuid(
        client_broker_dealer,
        question_uuid,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_zmq_send(
        client_broker_dealer,
        question_payload,
        sizeof(question_payload),
        0));  // flags

    // FIXME(strager): This method sucks!
    B_LOG(B_INFO, "Waiting for worker to pick up request.");
    sleep(1);

    // Receive client request.
    void *worker_broker_dealer;
    B_CHECK_EX(b_zmq_socket(
        context_zmq,
        ZMQ_DEALER,
        &worker_broker_dealer));
    B_ZMQSocketScope worker_broker_dealer_scope(
        worker_broker_dealer);
    B_CHECK_EX(b_protocol_connectbind_worker(
        worker_broker_dealer,
        broker_address,
        B_CONNECT));

    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_READY,
        0,  // flags
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_protocol_recv_identity_delimiter(
        worker_broker_dealer,
        0));  // flags

    B_Identity *client_identity
        = b_protocol_recv_identity_envelope(
            worker_broker_dealer,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_EXPECT_RECV_REQUEST_ID_EQ(
        ((B_RequestID) {{0, 1, 2, 3}}),
        worker_broker_dealer);

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            worker_broker_dealer,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(
            b_uuid_equal(question_uuid, received_uuid));
    }

    B_EXPECT_RECV(
        question_payload,
        sizeof(question_payload),
        worker_broker_dealer,
        0);  // flags

    // Send worker response.
    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_DONE_AND_EXIT,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    b_protocol_send_identity_envelope(
        worker_broker_dealer,
        client_identity,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    {
        B_RequestID request_id = {{0, 1, 2, 3}};
        B_CHECK_EX(b_protocol_send_request_id(
            worker_broker_dealer,
            &request_id,
            ZMQ_SNDMORE));
    }

    B_CHECK_EX(b_zmq_send(
        worker_broker_dealer,
        answer_payload,
        sizeof(answer_payload),
        0));  // flags

    // Receive worker response.
    B_CHECK_EX(b_protocol_recv_identity_delimiter(
        client_broker_dealer,
        0));  // flags

    B_EXPECT_RECV_REQUEST_ID_EQ(
        ((B_RequestID) {{0, 1, 2, 3}}),
        client_broker_dealer);

    B_EXPECT_RECV(
        answer_payload,
        sizeof(answer_payload),
        client_broker_dealer,
        0);  // flags
}

TEST(TestBroker, WorkAfterWorker) {
    B_Exception *ex = NULL;

    static B_UUID const question_uuid
        = B_UUID("F5114112-F33B-4A48-882C-CB1E5393A471");
    static uint8_t const question_payload[] = "hello world";
    static uint8_t const answer_payload[] = "g'day to you";

    void *context_zmq = zmq_ctx_new();
    ASSERT_NE(nullptr, context_zmq);
    B_ZMQContextScope context_zmq_scope(context_zmq);

    B_BrokerAddress *broker_address;
    B_CHECK_EX(create_broker_thread(
        context_zmq,
        &broker_address));
    sleep(1);

    // Create worker and mark as ready.
    void *worker_broker_dealer;
    B_CHECK_EX(b_zmq_socket(
        context_zmq,
        ZMQ_DEALER,
        &worker_broker_dealer));
    B_ZMQSocketScope worker_broker_dealer_scope(
        worker_broker_dealer);
    B_CHECK_EX(b_protocol_connectbind_worker(
        worker_broker_dealer,
        broker_address,
        B_CONNECT));

    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_READY,
        0,  // flags
        &ex);
    B_CHECK_EX(ex);

    // FIXME(strager): This method sucks!
    B_LOG(B_INFO, "Waiting for worker to pick up worker READY.");
    sleep(1);

    // Send client request.
    void *client_broker_dealer;
    B_CHECK_EX(b_zmq_socket(
        context_zmq,
        ZMQ_DEALER,
        &client_broker_dealer));
    B_ZMQSocketScope client_broker_dealer_scope(
        client_broker_dealer);
    B_CHECK_EX(b_protocol_connectbind_client(
        client_broker_dealer,
        broker_address,
        B_CONNECT));

    B_CHECK_EX(b_protocol_send_identity_delimiter(
        client_broker_dealer,
        ZMQ_SNDMORE));

    {
        B_RequestID request_id = {{0, 1, 2, 3}};
        B_CHECK_EX(b_protocol_send_request_id(
            client_broker_dealer,
            &request_id,
            ZMQ_SNDMORE));
    }

    b_protocol_send_uuid(
        client_broker_dealer,
        question_uuid,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_zmq_send(
        client_broker_dealer,
        question_payload,
        sizeof(question_payload),
        0));  // flags

    // Receive client request.
    B_CHECK_EX(b_protocol_recv_identity_delimiter(
        worker_broker_dealer,
        0));  // flags

    B_Identity *client_identity
        = b_protocol_recv_identity_envelope(
            worker_broker_dealer,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_EXPECT_RECV_REQUEST_ID_EQ(
        ((B_RequestID) {{0, 1, 2, 3}}),
        worker_broker_dealer);

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            worker_broker_dealer,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(
            b_uuid_equal(question_uuid, received_uuid));
    }

    B_EXPECT_RECV(
        question_payload,
        sizeof(question_payload),
        worker_broker_dealer,
        0);  // flags

    // Send worker response.
    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_DONE_AND_EXIT,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    b_protocol_send_identity_envelope(
        worker_broker_dealer,
        client_identity,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    {
        B_RequestID request_id = {{0, 1, 2, 3}};
        B_CHECK_EX(b_protocol_send_request_id(
            worker_broker_dealer,
            &request_id,
            ZMQ_SNDMORE));
    }

    B_CHECK_EX(b_zmq_send(
        worker_broker_dealer,
        answer_payload,
        sizeof(answer_payload),
        0));  // flags

    // Receive worker response.
    B_CHECK_EX(b_protocol_recv_identity_delimiter(
        client_broker_dealer,
        0));  // flags

    B_EXPECT_RECV_REQUEST_ID_EQ(
        ((B_RequestID) {{0, 1, 2, 3}}),
        client_broker_dealer);

    B_EXPECT_RECV(
        answer_payload,
        sizeof(answer_payload),
        client_broker_dealer,
        0);  // flags
}

TEST(TestBroker, WorkerExitsWithoutWork) {
    B_Exception *ex = NULL;

    void *context_zmq = zmq_ctx_new();
    ASSERT_NE(nullptr, context_zmq);
    B_ZMQContextScope context_zmq_scope(context_zmq);

    B_BrokerAddress *broker_address;
    B_CHECK_EX(create_broker_thread(
        context_zmq,
        &broker_address));
    sleep(1);

    // Create worker and mark as ready.
    void *worker_broker_dealer;
    B_CHECK_EX(b_zmq_socket(
        context_zmq,
        ZMQ_DEALER,
        &worker_broker_dealer));
    B_ZMQSocketScope worker_broker_dealer_scope(
        worker_broker_dealer);
    B_CHECK_EX(b_protocol_connectbind_worker(
        worker_broker_dealer,
        broker_address,
        B_CONNECT));

    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_READY,
        0,  // flags
        &ex);
    B_CHECK_EX(ex);

    // FIXME(strager): This method sucks!
    B_LOG(B_INFO, "Waiting for worker to pick up worker READY.");
    sleep(1);

    // Send EXIT request.
    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_EXIT,
        0,  // flags
        &ex);
    B_CHECK_EX(ex);

    // Await response.
    {
        uint8_t const expected_response[0] = {};
        B_EXPECT_RECV(
            expected_response,
            sizeof(expected_response),
            worker_broker_dealer,
            0);  // flags
    }
}
