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

TEST(TestBroker, WorkBeforeWorker) {
    B_Exception *ex;

    static B_UUID const question_uuid
        = B_UUID("F5114112-F33B-4A48-882C-CB1E5393A471");
    static uint8_t const question_payload[] = "hello world";
    static uint8_t const answer_payload[] = "g'day to you";

    void *context_zmq = zmq_ctx_new();
    ASSERT_NE(nullptr, context_zmq);

    B_Broker *broker;
    B_CHECK_EX(b_broker_allocate_bind(
        context_zmq,
        NULL,  // TODO(strager)
        NULL,  // TODO(strager)
        &broker));

    b_create_thread(
        "broker",
        [broker]() {
            B_Exception *ex = b_broker_run(broker);
            EXPECT_EQ(nullptr, ex);
            if (ex) {
                B_LOG_EXCEPTION(ex);
            }
        });

    // Send client request.
    void *client_broker_dealer;
    B_CHECK_EX(b_protocol_connect_client(
        context_zmq,
        broker,
        ZMQ_DEALER,
        &client_broker_dealer));

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

    // Receive the request.
    void *worker_broker_dealer;
    B_CHECK_EX(b_protocol_connect_worker(
        context_zmq,
        broker,
        ZMQ_DEALER,
        &worker_broker_dealer));

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

    // Send response.
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

    // Receive response.
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
