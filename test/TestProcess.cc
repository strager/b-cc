#include <B/Assert.h>
#include <B/Config.h>
#include <B/Process.h>
#include <B/Thread.h>

#include <gtest/gtest.h>
#include <list>
#include <stdint.h>
#include <sys/time.h>

static uint64_t
timeval_microseconds_(
        struct timeval tv) {
    return tv.tv_usec
        + (uint64_t)tv.tv_sec * (uint64_t)(1000 * 1000);
}

static uint64_t const
g_variance_microseconds_ = 500 * 1000;  // 500ms

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
            B_OUTPTR bool *received_exit_code,
            B_OUTPTR int *exit_code) {
        ExecClosure_ **signaled_closure;
        ExecClosure_ *self = this;
        await_many(
            &self,
            &self + 1,
            &signaled_closure,
            received_exit_code,
            exit_code);
        B_ASSERT(*signaled_closure == this);
    }

    template<typename TIter>
    static void
    await_many(
            TIter const &begin,
            TIter const &end,
            B_OUTPTR TIter *signaled_closure,
            B_OUTPTR bool *received_exit_code,
            B_OUTPTR int *exit_code) {
        B_ASSERT(begin != end);

        for (TIter i = begin; i != end; ++i) {
            ExecClosure_ *closure = *i;
            B_ASSERT(closure->did_exec);
        }

#if defined(B_CONFIG_PTHREAD)
        {
            B_PthreadMutexHolder locker(&shared_lock);

            for (;;) {
                for (TIter i = begin; i != end; ++i) {
                    ExecClosure_ *closure = *i;
                    if (closure->callback_called) {
                        *signaled_closure = i;
                        *received_exit_code
                            = closure->received_exit_code;
                        *exit_code = closure->exit_code;
                        return;
                    }
                }

                int rc = pthread_cond_wait(
                    &shared_cond,
                    &shared_lock);
                /*
                 * NOTE(strager): We can't ASSERT_EQ here as
                 * that would leave output variables
                 * uninitializezed.
                 */
                EXPECT_EQ(0, rc);
                B_ASSERT(rc == 0);
            }
        }
#else
# error "Unknown threads implementation"
#endif
    }

private:
#if defined(B_CONFIG_PTHREAD)
    static pthread_mutex_t shared_lock;
    static pthread_cond_t shared_cond;
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
            B_PthreadMutexHolder locker(&shared_lock);
#endif
            B_ASSERT(!closure->callback_called);
            closure->received_exit_code = true;
            closure->exit_code = exit_code;
            closure->callback_called = true;
#if defined(B_CONFIG_PTHREAD)
            int rc = pthread_cond_signal(&shared_cond);
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
            B_PthreadMutexHolder locker(&shared_lock);
#endif
            B_ASSERT(!closure->callback_called);
            closure->received_exit_code = false;
            closure->callback_called = true;
#if defined(B_CONFIG_PTHREAD)
            int rc = pthread_cond_signal(&shared_cond);
            B_ASSERT(rc == 0);
#endif
            return true;
        }
    }
};

#if defined(B_CONFIG_PTHREAD)
pthread_mutex_t
ExecClosure_::shared_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t
ExecClosure_::shared_cond = PTHREAD_COND_INITIALIZER;
#endif

TEST(TestProcess, AllocateDeallocate) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));
    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, AllocateAsyncDeallocate) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));
    EXPECT_TRUE(b_process_loop_run_async_unsafe(loop, eh));
    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, RunAsyncTrue) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));
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

TEST(TestProcess, RunAsyncFalse) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));
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

TEST(TestProcess, RunTrueAsync) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));

    ExecClosure_ exec_closure;

    char const *const args[] = {
        "true",
        nullptr,
    };
    ASSERT_TRUE(exec_closure.exec(loop, args, eh));

    ASSERT_TRUE(b_process_loop_run_async_unsafe(loop, eh));

    bool received_exit_code;
    int exit_code;
    exec_closure.await(&received_exit_code, &exit_code);
    EXPECT_TRUE(received_exit_code);
    if (received_exit_code) {
        EXPECT_EQ(0, exit_code);
    }

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, RunFalseAsync) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));

    ExecClosure_ exec_closure;

    char const *const args[] = {
        "false",
        nullptr,
    };
    ASSERT_TRUE(exec_closure.exec(loop, args, eh));

    ASSERT_TRUE(b_process_loop_run_async_unsafe(loop, eh));

    bool received_exit_code;
    int exit_code;
    exec_closure.await(&received_exit_code, &exit_code);
    EXPECT_TRUE(received_exit_code);
    if (received_exit_code) {
        EXPECT_EQ(1, exit_code);
    }

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, ReentrantStopSync) {
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));

    char const *const args[] = {
        "true",
        nullptr,
    };
    ASSERT_TRUE(b_process_loop_exec(
        loop,
        args,
        [](
                int,
                void *opaque,
                B_ErrorHandler const *eh) {
            B_ProcessLoop *loop
                = static_cast<B_ProcessLoop *>(opaque);
            bool ok = b_process_loop_stop(loop, eh);
            EXPECT_TRUE(ok);
            return ok;
        },
        [](
                B_Error,
                void *,
                B_ErrorHandler const *) {
            ADD_FAILURE();
            return true;
        },
        loop,
        eh));

    ASSERT_TRUE(b_process_loop_run_sync(loop, eh));

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

struct ExecReentrantStopClosure_ {
    ExecReentrantStopClosure_(
            B_ProcessLoop *loop) :
            loop(loop),
            called(false) {
    }

    B_ProcessLoop *loop;
    bool called;
};

TEST(TestProcess, ReentrantStopSyncWithOthersRunning) {
    // Launch more than one process and ensures a stop will
    // prevent calls to callbacks for other processes.

    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));

    char const *const args[] = {
        "true",
        nullptr,
    };

    ExecReentrantStopClosure_ closure(loop);
    auto exit_callback = [](
            int,
            void *opaque,
            B_ErrorHandler const *eh) {
        auto *closure
            = static_cast<ExecReentrantStopClosure_ *>(
                opaque);
        B_ASSERT(closure);
        B_ASSERT(closure->loop);

        EXPECT_FALSE(closure->called);
        closure->called = true;

        bool ok = b_process_loop_stop(closure->loop, eh);
        EXPECT_TRUE(ok);
        return ok;
    };
    auto error_callback = [](
            B_Error,
            void *opaque,
            B_ErrorHandler const *) {
        auto *closure
            = static_cast<ExecReentrantStopClosure_ *>(
                opaque);
        B_ASSERT(closure);

        closure->called = true;
        ADD_FAILURE();
        return true;
    };

    for (size_t i = 0; i < 10; ++i) {
        ASSERT_TRUE(b_process_loop_exec(
            loop,
            args,
            exit_callback,
            error_callback,
            &closure,
            eh));
    }

    ASSERT_TRUE(b_process_loop_run_sync(loop, eh));

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, TenProcessesNoLimit) {
    size_t const process_count = 10;
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));
    ASSERT_TRUE(b_process_loop_run_async_unsafe(loop, eh));

    ExecClosure_ exec_closures[process_count];

    char const *const args[] = {
        "sleep",
        "3",
        nullptr,
    };
    for (size_t i = 0; i < process_count; ++i) {
        ASSERT_TRUE(exec_closures[i].exec(loop, args, eh));
    }

    for (size_t i = 0; i < process_count; ++i) {
        bool received_exit_code;
        int exit_code;
        exec_closures[i].await(
            &received_exit_code,
            &exit_code);
        EXPECT_TRUE(received_exit_code);
        if (received_exit_code) {
            EXPECT_EQ(0, exit_code);
        }
    }

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, TenProcessesThreeLimit) {
    size_t const process_count = 10;
    size_t const process_limit = 3;
    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        process_limit,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));
    ASSERT_TRUE(b_process_loop_run_async_unsafe(loop, eh));

    ExecClosure_ exec_closures[process_count];

    char const *const args[] = {
        "sleep",
        "3",
        nullptr,
    };
    for (size_t i = 0; i < process_count; ++i) {
        ASSERT_TRUE(exec_closures[i].exec(loop, args, eh));
    }

    std::list<ExecClosure_ *> remaining_closures;
    for (size_t i = 0; i < process_count; ++i) {
        remaining_closures.push_back(&exec_closures[i]);
    }

    uint64_t times_us[process_count];
    for (size_t i = 0; !remaining_closures.empty(); ++i) {
        std::list<ExecClosure_ *>::iterator
            signaled_closure;
        bool received_exit_code;
        int exit_code;
        ExecClosure_::await_many(
            remaining_closures.begin(),
            remaining_closures.end(),
            &signaled_closure,
            &received_exit_code,
            &exit_code);
        EXPECT_TRUE(received_exit_code);
        if (received_exit_code) {
            EXPECT_EQ(0, exit_code);
        }
        struct timeval tv;
        ASSERT_EQ(0, gettimeofday(&tv, nullptr));
        times_us[i] = timeval_microseconds_(tv);
        remaining_closures.erase(signaled_closure);
    }

    // Ensure times within a round match, and times across
    // rounds differ by three seconds.  E.g.
    //
    // 0: 10s
    // 1: 10s
    // 2: 10s
    // 3: 13s
    // 4: 13s
    // 5: 14s  // Bad!
    // 6: 16s
    size_t const round_count
        = (process_count + process_limit - 1)
        / process_limit;
    B_STATIC_ASSERT(
        round_count == 4,
        "round_count inaccurate; please change this assertion or fix the formula");
    for (size_t round = 0; round < round_count; ++round) {
        size_t round_start = round * process_limit;
        size_t round_end = std::min(
            round_start + process_limit,
            process_count);
        uint64_t first_time_us = times_us[round_start];

        EXPECT_NEAR(
            times_us[0]
                + round * (3 * 1000 * 1000 /* 3s */),
            first_time_us,
            g_variance_microseconds_)
            << "round " << round
            << " did not come " << round * 3 << "s"
            << " after round 0";
        for (size_t i = round_start; i < round_end; ++i) {
            EXPECT_NEAR(
                first_time_us,
                times_us[i],
                g_variance_microseconds_)
                << "round " << round << ", index " << i;
        }
    }

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

static B_FUNC
process_begets_process_exec_(
        ExecReentrantStopClosure_ *,
        B_ErrorHandler const *);

static B_FUNC
process_begets_process_exit_(
        int exit_code,
        void *opaque,
        B_ErrorHandler const *);

static B_FUNC
process_begets_process_error_(
        B_Error,
        void *opaque,
        B_ErrorHandler const *);

static B_FUNC
process_begets_process_exec_(
        ExecReentrantStopClosure_ *closure,
        B_ErrorHandler const *eh) {
    B_ASSERT(closure);
    B_ASSERT(closure->loop);

    char const *const args[] = {
        "true",
        nullptr,
    };
    bool ok = b_process_loop_exec(
        closure->loop,
        args,
        process_begets_process_exit_,
        process_begets_process_error_,
        closure + 1,
        eh);
    EXPECT_TRUE(ok);
    return ok;
}

static B_FUNC
process_begets_process_exit_(
        int,
        void *opaque,
        B_ErrorHandler const *eh) {
    auto *closure
        = static_cast<ExecReentrantStopClosure_ *>(opaque);
    B_ASSERT(closure);

    EXPECT_FALSE(closure->called);
    closure->called = true;

    bool ok = true;
    if (closure->loop) {
        ok = process_begets_process_exec_(closure, eh)
            && ok;
    } else {
        // HACK(strager)
        ok = b_process_loop_stop(closure[-1].loop, eh)
            && ok;
    }
    EXPECT_TRUE(ok);
    return ok;
}

static B_FUNC
process_begets_process_error_(
        B_Error,
        void *opaque,
        B_ErrorHandler const *) {
    auto *closure
        = static_cast<ExecReentrantStopClosure_ *>(opaque);
    B_ASSERT(closure);

    closure->called = true;
    ADD_FAILURE();
    return true;
}

TEST(TestProcess, ProcessBegetsProcess) {
    size_t const nest_level = 10;

    B_ErrorHandler const *eh = nullptr;

    B_ProcessLoop *loop;
    ASSERT_TRUE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        &loop,
        eh));

    ExecReentrantStopClosure_ closures[nest_level + 1] = {
        loop, loop, loop, loop, loop,
        loop, loop, loop, loop, loop,
        nullptr,
    };

    ASSERT_TRUE(process_begets_process_exec_(
        &closures[0],
        eh));

    ASSERT_TRUE(b_process_loop_run_sync(loop, eh));

    EXPECT_TRUE(b_process_loop_deallocate(loop, 0, eh));
}

TEST(TestProcess, AllocateInvalidParameter) {
    B_ErrorHandler const *eh = nullptr;

    ASSERT_FALSE(b_process_loop_allocate(
        0,
        b_process_auto_configuration_unsafe(),
        nullptr,
        eh));
}
