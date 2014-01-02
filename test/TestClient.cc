#include <B/Answer.h>
#include <B/Broker.h>
#include <B/Client.h>
#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/GTest.h>
#include <B/Internal/Identity.h>
#include <B/Internal/Portable.h>
#include <B/Internal/PortableSignal.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/VTable.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>
#include <B/Question.h>
#include <B/UUID.h>

#include <gtest/gtest.h>
#include <zmq.h>

struct B_ClientScope {
    B_ClientScope(
        B_Client *client) :
        client(client) {
    }

    ~B_ClientScope() {
        B_CHECK_EX(b_client_deallocate_disconnect(client));
    }

    B_Client *client;
};

namespace test_vtables {

B_DEFINE_VTABLE_FUNCTION(B_AnswerVTable, answer_vtable, {
    .equal = [](
        B_AnyAnswer const *,
        B_AnyAnswer const *) -> bool {
        assert(0);
    },

    .replicate = [](
        B_AnyAnswer const *) -> B_AnyAnswer * {
        assert(0);
    },

    .deallocate = [](
        B_AnyAnswer *) {
        assert(0);
    },

    .serialize = [](
        void const *value,
        B_Serializer,
        void *serializer_closure) {
        assert(0);
    },

    .deserialize = [](
        B_Deserializer deserialize,
        void *deserializer_closure) -> void * {

        uint8_t buffer[6];
        size_t read_size = deserialize(
            reinterpret_cast<char *>(&buffer),
            sizeof(buffer),
            deserializer_closure);
        EXPECT_EQ(5, read_size);

        return new std::vector<uint8_t>(
            buffer,
            buffer + read_size);
    },
});

B_DEFINE_VTABLE_FUNCTION(B_QuestionVTable, question_vtable, {
    .uuid = B_UUID("37055857-AD12-4B29-896B-3AF0B70EA59F"),
    .answer_vtable = answer_vtable(),

    .answer = [](
        B_AnyQuestion const *,
        B_Exception **) -> B_AnyAnswer * {
        assert(0);
    },

    .equal = [](
        B_AnyQuestion const *,
        B_AnyQuestion const *) -> bool {
        assert(0);
    },

    .replicate = [](
        B_AnyQuestion const *) -> B_AnyQuestion * {
        assert(0);
    },

    .deallocate = [](
        B_AnyQuestion *) {
        assert(0);
    },

    .allocate_human_message = [](
        B_AnyQuestion const *) -> char const * {
        return "<mock question>";
    },

    .deallocate_human_message = [](
        char const *) {
        // Do nothing.
    },

    .serialize = [](
        void const *value,
        B_Serializer serializer,
        void *serializer_closure) {

        uint8_t varied_value
            = reinterpret_cast<uintptr_t>(value);
        uint8_t bytes[] = {5, 3, varied_value};
        serializer(
            reinterpret_cast<char const *>(bytes),
            sizeof(bytes),
            serializer_closure);
    },

    .deserialize = [](
        B_Deserializer,
        void *deserializer_closure) -> void * {
        assert(0);
    },
});

}

static void
NeedAnswerOne_fake_broker_and_worker(
    void *context_zmq,
    B_BrokerAddress *broker_address) {

    struct B_Exception *ex = NULL;

    void *broker_client_router;
    B_CHECK_EX(b_protocol_connectbind_client(
        context_zmq,
        broker_address,
        ZMQ_ROUTER,
        B_BIND,
        &broker_client_router));
    B_ZMQSocketScope broker_client_router_scope(
        broker_client_router);

    // Receive request.
    B_Identity *client_identity
        = b_protocol_recv_identity_envelope(
            broker_client_router,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_RequestID request_id;
    B_CHECK_EX(b_protocol_recv_request_id(
        broker_client_router,
        &request_id,
        0));  /* flags */

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            broker_client_router,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(b_uuid_equal(
            test_vtables::question_vtable()->uuid,
            received_uuid));
    }

    {
        uint8_t const expected_question_data[] = {5, 3, 123};
        B_EXPECT_RECV(
            expected_question_data,
            sizeof(expected_question_data),
            broker_client_router,
            0);  // flags
    }

    // Send response.
    b_protocol_send_identity_envelope(
        broker_client_router,
        client_identity,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_protocol_send_request_id(
        broker_client_router,
        &request_id,
        ZMQ_SNDMORE));

    uint8_t const answer_data[] = {10, 3, 4, 8, 177};
    B_CHECK_EX(b_zmq_send(
        broker_client_router,
        answer_data,
        sizeof(answer_data),
        0));  // flags
}

TEST(TestClient, NeedAnswerOne) {
    B_Signal finished_signal;
    B_CHECK_EX(b_signal_init(&finished_signal));

    B_MultithreadAssert mt_assert;

    {
        void *context_zmq = zmq_ctx_new();
        ASSERT_NE(nullptr, context_zmq);
        B_ZMQContextScope context_zmq_scope(context_zmq);

        B_BrokerAddress *broker_address;
        B_CHECK_EX(b_broker_address_allocate(
            &broker_address));

        B_CHECK_EX(b_create_thread(
            "fake_broker_and_worker",
            [&]() {
                mt_assert.capture([&]{
                    NeedAnswerOne_fake_broker_and_worker(
                        context_zmq,
                        broker_address);
                });

                // Tell the main thread we've finished.
                B_Exception *ex
                    = b_signal_raise(&finished_signal);
                assert(!ex);
            }));

        B_FiberContext *fiber_context;
        B_CHECK_EX(b_fiber_context_allocate(
            &fiber_context));

        B_Client *client;
        B_CHECK_EX(b_client_allocate_connect(
            context_zmq,
            broker_address,
            fiber_context,
            &client));
        B_ClientScope client_scope(client);

        // HACK(strager): Work around ZeroMQ bug.
        B_LOG(B_INFO, "Waiting for client to connect.");
        sleep(1);
        B_MT_ASSERT_CHECK(mt_assert);

        B_AnyQuestion *question
            = reinterpret_cast<B_AnyQuestion *>(123);
        B_AnyAnswer *answer;
        B_CHECK_EX(b_client_need_answer_one(
            client,
            question,
            test_vtables::question_vtable(),
            &answer));
        B_MT_ASSERT_CHECK(mt_assert);

        auto answer_bytes
            = reinterpret_cast<std::vector<uint8_t> *>(
                answer);
        uint8_t const expected_answer_data[]
            = {10, 3, 4, 8, 177};
        B_EXPECT_MEMEQ(
            expected_answer_data,
            sizeof(expected_answer_data),
            answer_bytes->data(),
            answer_bytes->size());
        delete answer_bytes;

    }  // Kill ZeroMQ context.

    B_LOG(B_INFO, "Waiting for thread to die.");
    B_CHECK_EX(b_signal_await(&finished_signal));

    B_MT_ASSERT_CHECK(mt_assert);
}

static void
NeedAnswerOneNested_fake_broker_and_worker(
    void *context_zmq,
    B_BrokerAddress *broker_address) {

    struct B_Exception *ex = NULL;

    void *broker_client_router;
    B_CHECK_EX(b_protocol_connectbind_client(
        context_zmq,
        broker_address,
        ZMQ_ROUTER,
        B_BIND,
        &broker_client_router));
    B_ZMQSocketScope broker_client_router_scope(
        broker_client_router);

    // Receive request 1.
    B_Identity *client_identity1
        = b_protocol_recv_identity_envelope(
            broker_client_router,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_RequestID request_id1;
    B_CHECK_EX(b_protocol_recv_request_id(
        broker_client_router,
        &request_id1,
        0));  /* flags */

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            broker_client_router,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(b_uuid_equal(
            test_vtables::question_vtable()->uuid,
            received_uuid));
    }

    {
        uint8_t const expected_question_data[] = {5, 3, 123};
        B_EXPECT_RECV(
            expected_question_data,
            sizeof(expected_question_data),
            broker_client_router,
            0);  // flags
    }

    // Receive request 2.
    B_Identity *client_identity2
        = b_protocol_recv_identity_envelope(
            broker_client_router,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_EXPECT_MEMEQ(
        client_identity1->data,
        client_identity1->data_size,
        client_identity2->data,
        client_identity2->data_size);
        // << "Client identity should be the same for both requests";

    B_RequestID request_id2;
    B_CHECK_EX(b_protocol_recv_request_id(
        broker_client_router,
        &request_id2,
        0));  /* flags */

    B_EXPECT_REQUEST_ID_NE(request_id1, request_id2);
        // << "Client requests should differ.";

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            broker_client_router,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(b_uuid_equal(
            test_vtables::question_vtable()->uuid,
            received_uuid));
    }

    {
        uint8_t const expected_question_data[] = {5, 3, 97};
        B_EXPECT_RECV(
            expected_question_data,
            sizeof(expected_question_data),
            broker_client_router,
            0);  // flags
    }

    // Send response 1.
    b_protocol_send_identity_envelope(
        broker_client_router,
        client_identity1,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_protocol_send_request_id(
        broker_client_router,
        &request_id1,
        ZMQ_SNDMORE));

    uint8_t const answer_data1[] = {10, 3, 4, 8, 177};
    B_CHECK_EX(b_zmq_send(
        broker_client_router,
        answer_data1,
        sizeof(answer_data1),
        0));  // flags

    // Send response 2.
    b_protocol_send_identity_envelope(
        broker_client_router,
        client_identity2,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_protocol_send_request_id(
        broker_client_router,
        &request_id2,
        ZMQ_SNDMORE));

    uint8_t const answer_data2[]
        = {42, 33, 4, 199, 2};
    B_CHECK_EX(b_zmq_send(
        broker_client_router,
        answer_data2,
        sizeof(answer_data2),
        0));  // flags
}

TEST(TestClient, NeedAnswerOneNested) {
    B_Signal finished_signal;
    B_CHECK_EX(b_signal_init(&finished_signal));

    B_MultithreadAssert mt_assert;

    {
        void *context_zmq = zmq_ctx_new();
        ASSERT_NE(nullptr, context_zmq);
        B_ZMQContextScope context_zmq_scope(context_zmq);

        B_BrokerAddress *broker_address;
        B_CHECK_EX(b_broker_address_allocate(
            &broker_address));

        B_CHECK_EX(b_create_thread(
            "fake_broker_and_worker",
            [&]() {
                mt_assert.capture([&]{
                    NeedAnswerOneNested_fake_broker_and_worker(
                        context_zmq,
                        broker_address);
                });

                // Tell the main thread we've finished.
                B_Exception *ex
                    = b_signal_raise(&finished_signal);
                assert(!ex);
            }));

        B_FiberContext *fiber_context;
        B_CHECK_EX(b_fiber_context_allocate(
            &fiber_context));

        B_Client *client;
        B_CHECK_EX(b_client_allocate_connect(
            context_zmq,
            broker_address,
            fiber_context,
            &client));
        B_ClientScope client_scope(client);

        // HACK(strager): Work around ZeroMQ bug.
        B_LOG(B_INFO, "Waiting for client to connect.");
        sleep(1);
        B_MT_ASSERT_CHECK(mt_assert);

        B_AnyQuestion *question1
            = reinterpret_cast<B_AnyQuestion *>(123);
        B_AnyQuestion *question2
            = reinterpret_cast<B_AnyQuestion *>(97);
        B_AnyAnswer *answer1;
        B_AnyAnswer *answer2;

        bool volatile fork_finished = false;
        B_CHECK_EX(b_fiber_context_fork(
            fiber_context,
            [&]() {
                B_Exception *ex = b_client_need_answer_one(
                    client,
                    question2,
                    test_vtables::question_vtable(),
                    &answer2);
                fork_finished = true;
                B_CHECK_EX(ex);
            }));
        B_CHECK_EX(b_client_need_answer_one(
            client,
            question1,
            test_vtables::question_vtable(),
            &answer1));
        B_MT_ASSERT_CHECK(mt_assert);

        B_LOG(B_INFO, "Waiting for fork to finish.");
        while (!fork_finished) {
            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
        }
        B_MT_ASSERT_CHECK(mt_assert);

        {
            auto answer_bytes1
                = reinterpret_cast<std::vector<uint8_t> *>(
                    answer1);
            uint8_t const expected_answer_data1[]
                = {10, 3, 4, 8, 177};
            B_EXPECT_MEMEQ(
                expected_answer_data1,
                sizeof(expected_answer_data1),
                answer_bytes1->data(),
                answer_bytes1->size());
            delete answer_bytes1;
        }

        {
            auto answer_bytes2
                = reinterpret_cast<std::vector<uint8_t> *>(
                    answer2);
            uint8_t const expected_answer_data2[]
                = {42, 33, 4, 199, 2};
            B_EXPECT_MEMEQ(
                expected_answer_data2,
                sizeof(expected_answer_data2),
                answer_bytes2->data(),
                answer_bytes2->size());
            delete answer_bytes2;
        }
    }  // Kill ZeroMQ context.

    B_LOG(B_INFO, "Waiting for thread to die.");
    B_CHECK_EX(b_signal_await(&finished_signal));
    B_MT_ASSERT_CHECK(mt_assert);
}

static void
NeedAnswerOneInverseNested_fake_broker_and_worker(
    void *context_zmq,
    B_BrokerAddress *broker_address) {

    struct B_Exception *ex = NULL;

    void *broker_client_router;
    B_CHECK_EX(b_protocol_connectbind_client(
        context_zmq,
        broker_address,
        ZMQ_ROUTER,
        B_BIND,
        &broker_client_router));
    B_ZMQSocketScope broker_client_router_scope(
        broker_client_router);

    // Receive request 1.
    B_Identity *client_identity1
        = b_protocol_recv_identity_envelope(
            broker_client_router,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_RequestID request_id1;
    B_CHECK_EX(b_protocol_recv_request_id(
        broker_client_router,
        &request_id1,
        0));  /* flags */

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            broker_client_router,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(b_uuid_equal(
            test_vtables::question_vtable()->uuid,
            received_uuid));
    }

    {
        uint8_t const expected_question_data[] = {5, 3, 123};
        B_EXPECT_RECV(
            expected_question_data,
            sizeof(expected_question_data),
            broker_client_router,
            0);  // flags
    }

    // Receive request 2.
    B_Identity *client_identity2
        = b_protocol_recv_identity_envelope(
            broker_client_router,
            0,  // flags
            &ex);
    B_CHECK_EX(ex);

    B_EXPECT_MEMEQ(
        client_identity1->data,
        client_identity1->data_size,
        client_identity2->data,
        client_identity2->data_size);
        // << "Client identity should be the same for both requests";

    B_RequestID request_id2;
    B_CHECK_EX(b_protocol_recv_request_id(
        broker_client_router,
        &request_id2,
        0));  /* flags */

    B_EXPECT_REQUEST_ID_NE(request_id1, request_id2);
        // << "Client requests should differ.";

    {
        B_UUID received_uuid = b_protocol_recv_uuid(
            broker_client_router,
            0,  // flags
            &ex);
        B_CHECK_EX(ex);

        EXPECT_TRUE(b_uuid_equal(
            test_vtables::question_vtable()->uuid,
            received_uuid));
    }

    {
        uint8_t const expected_question_data[] = {5, 3, 97};
        B_EXPECT_RECV(
            expected_question_data,
            sizeof(expected_question_data),
            broker_client_router,
            0);  // flags
    }

    // Send response 2.
    b_protocol_send_identity_envelope(
        broker_client_router,
        client_identity2,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_protocol_send_request_id(
        broker_client_router,
        &request_id2,
        ZMQ_SNDMORE));

    uint8_t const answer_data2[]
        = {42, 33, 4, 199, 2};
    B_CHECK_EX(b_zmq_send(
        broker_client_router,
        answer_data2,
        sizeof(answer_data2),
        0));  // flags

    // Send response 1.
    b_protocol_send_identity_envelope(
        broker_client_router,
        client_identity1,
        ZMQ_SNDMORE,
        &ex);
    B_CHECK_EX(ex);

    B_CHECK_EX(b_protocol_send_request_id(
        broker_client_router,
        &request_id1,
        ZMQ_SNDMORE));

    uint8_t const answer_data1[] = {10, 3, 4, 8, 177};
    B_CHECK_EX(b_zmq_send(
        broker_client_router,
        answer_data1,
        sizeof(answer_data1),
        0));  // flags

}

TEST(TestClient, NeedAnswerOneInverseNested) {
    B_Signal finished_signal;
    B_CHECK_EX(b_signal_init(&finished_signal));

    B_MultithreadAssert mt_assert;

    {
        void *context_zmq = zmq_ctx_new();
        ASSERT_NE(nullptr, context_zmq);
        B_ZMQContextScope context_zmq_scope(context_zmq);

        B_BrokerAddress *broker_address;
        B_CHECK_EX(b_broker_address_allocate(
            &broker_address));

        B_CHECK_EX(b_create_thread(
            "fake_broker_and_worker",
            [&]() {
                mt_assert.capture([&]{
                    NeedAnswerOneInverseNested_fake_broker_and_worker(
                        context_zmq,
                        broker_address);
                });

                // Tell the main thread we've finished.
                B_Exception *ex
                    = b_signal_raise(&finished_signal);
                assert(!ex);
            }));

        B_FiberContext *fiber_context;
        B_CHECK_EX(b_fiber_context_allocate(
            &fiber_context));

        B_Client *client;
        B_CHECK_EX(b_client_allocate_connect(
            context_zmq,
            broker_address,
            fiber_context,
            &client));
        B_ClientScope client_scope(client);

        // HACK(strager): Work around ZeroMQ bug.
        B_LOG(B_INFO, "Waiting for client to connect.");
        sleep(1);
        B_MT_ASSERT_CHECK(mt_assert);

        B_AnyQuestion *question1
            = reinterpret_cast<B_AnyQuestion *>(123);
        B_AnyQuestion *question2
            = reinterpret_cast<B_AnyQuestion *>(97);
        B_AnyAnswer *answer1;
        B_AnyAnswer *answer2;

        bool volatile fork_finished = false;
        B_CHECK_EX(b_fiber_context_fork(
            fiber_context,
            [&]() {
                B_Exception *ex = b_client_need_answer_one(
                    client,
                    question2,
                    test_vtables::question_vtable(),
                    &answer2);
                fork_finished = true;
                B_CHECK_EX(ex);
            }));
        B_CHECK_EX(b_client_need_answer_one(
            client,
            question1,
            test_vtables::question_vtable(),
            &answer1));
        B_MT_ASSERT_CHECK(mt_assert);

        B_LOG(B_INFO, "Waiting for fork to finish.");
        while (!fork_finished) {
            B_CHECK_EX(b_fiber_context_hard_yield(
                fiber_context));
        }
        B_MT_ASSERT_CHECK(mt_assert);

        {
            auto answer_bytes1
                = reinterpret_cast<std::vector<uint8_t> *>(
                    answer1);
            uint8_t const expected_answer_data1[]
                = {10, 3, 4, 8, 177};
            B_EXPECT_MEMEQ(
                expected_answer_data1,
                sizeof(expected_answer_data1),
                answer_bytes1->data(),
                answer_bytes1->size());
            delete answer_bytes1;
        }

        {
            auto answer_bytes2
                = reinterpret_cast<std::vector<uint8_t> *>(
                    answer2);
            uint8_t const expected_answer_data2[]
                = {42, 33, 4, 199, 2};
            B_EXPECT_MEMEQ(
                expected_answer_data2,
                sizeof(expected_answer_data2),
                answer_bytes2->data(),
                answer_bytes2->size());
            delete answer_bytes2;
        }
    }  // Kill ZeroMQ context.

    B_LOG(B_INFO, "Waiting for thread to die.");
    B_CHECK_EX(b_signal_await(&finished_signal));
    B_MT_ASSERT_CHECK(mt_assert);
}
