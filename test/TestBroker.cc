#include <B/Broker.h>
#include <B/Exception.h>
#include <B/Internal/Portable.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>
#include <B/UUID.h>

#include <zmq.h>

#include <gtest/gtest.h>

#include <iostream>

TEST(TestBroker, WorkBeforeWorker)
{
    B_Exception *ex;

    static B_UUID const question_uuid
        = B_UUID("F5114112-F33B-4A48-882C-CB1E5393A471");
    static uint8_t const question_payload[] = "hello world";
    static uint8_t const answer_payload[] = "g'day to you";

    void *context_zmq = zmq_ctx_new();
    ASSERT_NE(nullptr, context_zmq);

    B_Broker *broker;
    ex = b_broker_allocate_bind(
        context_zmq,
        NULL,  // TODO(strager)
        NULL,  // TODO(strager)
        &broker);
    ASSERT_EQ(nullptr, ex);

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
    ex = b_protocol_connect_client(
        context_zmq,
        broker,
        ZMQ_DEALER,
        &client_broker_dealer);
    ASSERT_EQ(nullptr, ex);

    ex = b_protocol_send_identity_delimiter(
        client_broker_dealer,
        ZMQ_SNDMORE);
    ASSERT_EQ(nullptr, ex);

    {
        B_RequestID request_id = {{0, 1, 2, 3}};
        ex = b_protocol_send_request_id(
            client_broker_dealer,
            &request_id,
            ZMQ_SNDMORE);
        ASSERT_EQ(nullptr, ex);
    }

    b_protocol_send_uuid(
        client_broker_dealer,
        question_uuid,
        ZMQ_SNDMORE,
        &ex);
    ASSERT_EQ(nullptr, ex);

    ex = b_zmq_send(
        client_broker_dealer,
        question_payload,
        sizeof(question_payload),
        0);  // flags
    ASSERT_EQ(nullptr, ex);

    // FIXME(strager): This method sucks!
    B_LOG(B_INFO, "Waiting for worker to pick up request.");
    sleep(1);

    // Receive the request.
    void *worker_broker_dealer;
    ex = b_protocol_connect_worker(
        context_zmq,
        broker,
        ZMQ_DEALER,
        &worker_broker_dealer);
    ASSERT_EQ(nullptr, ex);

    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_READY,
        0,  // flags
        &ex);
    ASSERT_EQ(nullptr, ex);

    ex = b_protocol_recv_identity_delimiter(
        worker_broker_dealer,
        0);  // flags
    ASSERT_EQ(nullptr, ex);

    B_Identity *client_identity
        = b_protocol_recv_identity_envelope(
            worker_broker_dealer,
            0,  // flags
            &ex);
    ASSERT_EQ(nullptr, ex);

    {
        B_RequestID received_request_id;
        ex = b_protocol_recv_request_id(
            worker_broker_dealer,
            &received_request_id,
            0);  // flags
        ASSERT_EQ(nullptr, ex);

        EXPECT_EQ(0, received_request_id.bytes[0]);
        EXPECT_EQ(1, received_request_id.bytes[1]);
        EXPECT_EQ(2, received_request_id.bytes[2]);
        EXPECT_EQ(3, received_request_id.bytes[3]);
    }

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            worker_broker_dealer,
            0,  // flags
            &ex);
        ASSERT_EQ(nullptr, ex);

        EXPECT_TRUE(
            b_uuid_equal(question_uuid, received_uuid));
    }

    {
        uint8_t received_question_payload
            [sizeof(question_payload) + 1] = {0};
        size_t received_question_payload_size
            = sizeof(question_payload);
        ex = b_zmq_recv(
            worker_broker_dealer,
            received_question_payload,
            &received_question_payload_size,
            0);  // flags
        ASSERT_EQ(nullptr, ex);

        EXPECT_EQ(
            sizeof(question_payload),
            received_question_payload_size);
        EXPECT_STREQ(
            reinterpret_cast<char const *>(question_payload),
            reinterpret_cast<char const *>(
                received_question_payload));
    }

    // Send response.
    b_protocol_send_worker_command(
        worker_broker_dealer,
        B_WORKER_DONE_AND_EXIT,
        ZMQ_SNDMORE,
        &ex);
    ASSERT_EQ(nullptr, ex);

    b_protocol_send_identity_envelope(
        worker_broker_dealer,
        client_identity,
        ZMQ_SNDMORE,
        &ex);
    ASSERT_EQ(nullptr, ex);

    {
        B_RequestID request_id = {{0, 1, 2, 3}};
        ex = b_protocol_send_request_id(
            worker_broker_dealer,
            &request_id,
            ZMQ_SNDMORE);
        ASSERT_EQ(nullptr, ex);
    }

    ex = b_zmq_send(
        worker_broker_dealer,
        answer_payload,
        sizeof(answer_payload),
        0);  // flags
    ASSERT_EQ(nullptr, ex);

    // Receive response.
    ex = b_protocol_recv_identity_delimiter(
        client_broker_dealer,
        0);  // flags
    ASSERT_EQ(nullptr, ex);

    {
        B_RequestID received_request_id;
        ex = b_protocol_recv_request_id(
            client_broker_dealer,
            &received_request_id,
            0);  // flags
        ASSERT_EQ(nullptr, ex);

        EXPECT_EQ(0, received_request_id.bytes[0]);
        EXPECT_EQ(1, received_request_id.bytes[1]);
        EXPECT_EQ(2, received_request_id.bytes[2]);
        EXPECT_EQ(3, received_request_id.bytes[3]);
    }

    {
        uint8_t received_answer_payload
            [sizeof(answer_payload) + 1] = {0};
        size_t received_answer_payload_size
            = sizeof(answer_payload);
        ex = b_zmq_recv(
            client_broker_dealer,
            received_answer_payload,
            &received_answer_payload_size,
            0);  // flags
        ASSERT_EQ(nullptr, ex);

        EXPECT_EQ(
            sizeof(answer_payload),
            received_answer_payload_size);
        EXPECT_STREQ(
            reinterpret_cast<char const *>(answer_payload),
            reinterpret_cast<char const *>(
                received_answer_payload));
    }
}
