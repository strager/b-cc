#include <B/Assert.h>
#include <B/Config.h>
#include <B/Process.h>
#include <B/Thread.h>

#include <gtest/gtest.h>

class ExecClosure_ {
public:
    ExecClosure_(
            ExecClosure_ const &) = delete;
    ExecClosure_(
            ExecClosure_ &&) = delete;
    ExecClosure_ &
    operator=(
            ExecClosure_ const &) = delete;
    ExecClosure_ &
    operator=(
            ExecClosure_ &&) = delete;

    ExecClosure_() :
#if defined(B_CONFIG_PTHREAD)
            lock(PTHREAD_MUTEX_INITIALIZER),
            cond(PTHREAD_COND_INITIALIZER),
#endif
            callback_called(false),
            received_exit_code(false),
            exit_code(0),
            did_exec(false) {
    }

    B_FUNC
    exec(
            B_ProcessLoop *loop,
            char const *const *args,
            B_ErrorHandler const *eh) {
        B_ASSERT(!this->did_exec);
        if (!b_process_loop_exec(
                loop,
                args,
                exit_callback,
                error_callback,
                this,
                eh)) {
            return false;
        }

        this->did_exec = true;
        return true;
    }

    void
    await(
            bool *received_exit_code,
            int *exit_code) {
        B_ASSERT(this->did_exec);
#if defined(B_CONFIG_PTHREAD)
        {
            B_PthreadMutexHolder locker(&this->lock);
            while (!this->callback_called) {
                int rc = pthread_cond_wait(
                    &this->cond,
                    &this->lock);
                ASSERT_EQ(0, rc);
            }
            *received_exit_code = this->received_exit_code;
            *exit_code = this->exit_code;
        }
#else
# error "Unknown threads implementation"
#endif
    }

private:
#if defined(B_CONFIG_PTHREAD)
    pthread_mutex_t lock;
    pthread_cond_t cond;
#endif
    bool callback_called;
    bool received_exit_code;
    int exit_code;
    bool did_exec;

    static B_FUNC
    exit_callback(
            int exit_code,
            void *opaque,
            B_ErrorHandler const *) {
        auto closure = static_cast<ExecClosure_ *>(opaque);
        {
#if defined(B_CONFIG_PTHREAD)
            B_PthreadMutexHolder locker(&closure->lock);
#endif
            B_ASSERT(!closure->callback_called);
            closure->received_exit_code = true;
            closure->exit_code = exit_code;
            closure->callback_called = true;
#if defined(B_CONFIG_PTHREAD)
            int rc = pthread_cond_signal(&closure->cond);
            B_ASSERT(rc == 0);
#endif
            return true;
        }
    }

    static B_FUNC
    error_callback(
            B_Error,
            void *opaque,
            B_ErrorHandler const *) {
        auto closure = static_cast<ExecClosure_ *>(opaque);
        {
#if defined(B_CONFIG_PTHREAD)
            B_PthreadMutexHolder locker(&closure->lock);
#endif
            B_ASSERT(!closure->callback_called);
            closure->received_exit_code = false;
            closure->callback_called = true;
#if defined(B_CONFIG_PTHREAD)
            int rc = pthread_cond_signal(&closure->cond);
            B_ASSERT(rc == 0);
#endif
            return true;
        }
    }
};

TEST(TestProcess, AllocateDeallocate)
{
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(0, &loop, eh));
    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, AllocateAsyncDeallocate)
{
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(0, &loop, eh));
    EXPECT_TRUE(b_process_loop_run_async_unsafe(loop, eh));
    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, RunAsyncTrue)
{
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(0, &loop, eh));
    ASSERT_TRUE(b_process_loop_run_async_unsafe(loop, eh));

    ExecClosure_ exec_closure;

    char const *const args[] = {
        "true",
        nullptr,
    };
    ASSERT_TRUE(exec_closure.exec(loop, args, eh));

    bool received_exit_code;
    int exit_code;
    exec_closure.await(&received_exit_code, &exit_code);
    EXPECT_TRUE(received_exit_code);
    if (received_exit_code) {
        EXPECT_EQ(0, exit_code);
    }

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, RunAsyncFalse)
{
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(0, &loop, eh));
    ASSERT_TRUE(b_process_loop_run_async_unsafe(loop, eh));

    ExecClosure_ exec_closure;

    char const *const args[] = {
        "false",
        nullptr,
    };
    ASSERT_TRUE(exec_closure.exec(loop, args, eh));

    bool received_exit_code;
    int exit_code;
    exec_closure.await(&received_exit_code, &exit_code);
    EXPECT_TRUE(received_exit_code);
    if (received_exit_code) {
        EXPECT_EQ(1, exit_code);
    }

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}
