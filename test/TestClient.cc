#include <B/Answer.h>
#include <B/Broker.h>
#include <B/Client.h>
#include <B/Exception.h>
#include <B/Fiber.h>
#include <B/Internal/GTest.h>
#include <B/Internal/Portable.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/VTable.h>
#include <B/Internal/ZMQ.h>
#include <B/Log.h>
#include <B/Question.h>
#include <B/UUID.h>

#include <gtest/gtest.h>
#include <zmq.h>

#include <pthread.h>

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
        B_Deserializer,
        void *deserializer_closure) -> void * {
        assert(0);
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
        assert(0);
    },
    .deallocate_human_message = [](
        char const *) {
        assert(0);
    },
    .serialize = [](
        void const *value,
        B_Serializer,
        void *serializer_closure) {
        assert(0);
    },
    .deserialize = [](
        B_Deserializer,
        void *deserializer_closure) -> void * {
        assert(0);
    },
});

}

TEST(TestClient, NeedOne) {
    //B_Exception *ex = NULL;

    // Semaphore emulated using cond.  Spawned thread
    // signals the semaphore.  Main thread awaits the
    // signal.
    pthread_cond_t thread_finished_cond
        = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t thread_finished_mutex
        = PTHREAD_MUTEX_INITIALIZER;
    bool thread_finished = false;

    {
        void *context_zmq = zmq_ctx_new();
        ASSERT_NE(nullptr, context_zmq);
        B_ZMQContextScope context_zmq_scope(context_zmq);

        B_BrokerAddress *broker_address;
        B_CHECK_EX(b_broker_address_allocate(
            &broker_address));

        B_MultithreadAssert mt_assert;
        b_create_thread(
            "fake_broker_and_worker",
            [&]() {
                mt_assert.capture([&]{
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

                // Tell the main thread we've finished.
                int rc;
                rc = pthread_mutex_lock(
                    &thread_finished_mutex);
                assert(rc == 0);
                thread_finished = true;
                rc = pthread_mutex_unlock(
                    &thread_finished_mutex);
                assert(rc == 0);
                rc = pthread_cond_signal(
                    &thread_finished_cond);
                assert(rc == 0);
            });

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

        B_AnyQuestion *question
            = reinterpret_cast<B_AnyQuestion *>(123);
        B_CHECK_EX(b_client_need_one(
            client,
            question,
            test_vtables::question_vtable()));
    }  // Kill ZeroMQ context.

    B_LOG(B_INFO, "Waiting for thread to die.");
    int rc;
    rc = pthread_mutex_lock(&thread_finished_mutex);
    assert(rc == 0);
    while (!thread_finished) {
        rc = pthread_cond_wait(
            &thread_finished_cond,
            &thread_finished_mutex);
        assert(rc == 0);
    }
    rc = pthread_mutex_unlock(&thread_finished_mutex);
    assert(rc == 0);
}
