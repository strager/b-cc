#include "Mocks.h"

#include <B/Assert.h>
#include <B/Config.h>
#include <B/Private/Thread.h>
#include <B/Process.h>

#include <chrono>
#include <errno.h>
#include <gtest/gtest.h>
#include <list>
#include <memory>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#if defined(B_CONFIG_POSIX_SIGNALS)
# include <signal.h>
# include <unistd.h>
#endif

#if defined(B_CONFIG_KQUEUE)
# include <sys/event.h>
#endif

template<typename TDuration>
static struct timespec
duration_to_timespec_(
        TDuration duration) {
    using namespace std::chrono;

    auto ns = duration_cast<nanoseconds>(duration);
    auto s = duration_cast<seconds>(ns);  // Truncates.
    struct timespec time;
    time.tv_sec = s.count();
    time.tv_nsec = duration_cast<nanoseconds>(ns - s)
        .count();
    return time;
}

static void
exec_basic_(
        B_ProcessManager *manager,
        std::vector<char const *> args,
        B_OUT bool *exited,  // async
        B_OUT B_ProcessExitStatus *exit_status) {  // async
    B_ErrorHandler const *eh = nullptr;
    args.push_back(nullptr);
    B_ProcessController *controller;
    ASSERT_TRUE(b_process_manager_get_controller(
        manager, &controller, eh));
    ASSERT_TRUE(b_process_controller_exec_basic(
        controller,
        args.data(),
        [exited, exit_status](
                B_ProcessExitStatus const *in_exit_status,
                B_ErrorHandler const *) -> bool {
            *exited = true;
            *exit_status = *in_exit_status;
            return true;
        },
        eh));
}

#if 0

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

#endif

TEST(TestProcessVanilla, ValidDelegateImplementations) {
    B_ErrorHandler const *eh = nullptr;

    // register_process_id/unregister_process_id
    auto r_id = [](
            struct B_ProcessControllerDelegate *,
            B_ProcessID,
            struct B_ErrorHandler const *) -> bool {
        return false;
    };
    // register_process_handle/unregister_process_handle
    auto r_h = [](
            struct B_ProcessControllerDelegate *,
            void *,
            struct B_ErrorHandler const *) -> bool {
        return false;
    };

    struct TestCase {
        B_ProcessControllerDelegate delegate;
        bool should_succeed;
    };
    TestCase test_cases[] = {
        { { nullptr, nullptr, nullptr, nullptr }, false },
        { { r_id,    r_id,    nullptr, nullptr }, true  },
        { { nullptr, nullptr, r_h,     r_h     }, true  },
        { { r_id,    r_id,    r_h,     r_h     }, true  },

        { { r_id,    nullptr, nullptr, nullptr }, false },
        { { r_id,    nullptr, r_h,     r_h     }, false },
        { { nullptr, r_id,    nullptr, nullptr }, false },
        { { nullptr, r_id,    r_h,     r_h     }, false },

        { { nullptr, nullptr, r_h,     nullptr }, false },
        { { nullptr, nullptr, nullptr, r_h     }, false },
        { { r_id,    r_id,    r_h,     nullptr }, false },
        { { r_id,    r_id,    nullptr, r_h     }, false },
    };

    for (TestCase test_case : test_cases) {
        if (test_case.should_succeed) {
            B_ProcessManager *manager;
            ASSERT_TRUE(b_process_manager_allocate(
                &test_case.delegate,
                &manager,
                eh));
            EXPECT_TRUE(b_process_manager_deallocate(
                manager, eh));
        } else {
            MockErrorHandler mock_eh;
            EXPECT_CALL(
                mock_eh, handle_error(B_Error{EINVAL}))
                .WillOnce(Return(B_ERROR_ABORT));

            B_ProcessManager *manager;
            EXPECT_FALSE(b_process_manager_allocate(
                &test_case.delegate, &manager, &mock_eh));
        }
    }
}

namespace Testers {

class Tester;

class Tester : public B_ProcessControllerDelegate {
public:
    class Factory {
    public:
        virtual std::unique_ptr<Tester>
        create_tester() const = 0;

        virtual const char *
        get_type_string() const = 0;
    };

    Tester() {
        this->register_process_id = nullptr;
        this->unregister_process_id = nullptr;
        this->register_process_handle = nullptr;
        this->unregister_process_handle = nullptr;
    }

    virtual ~Tester() {
    }

    // Waits for a child event, then calls the appropriate
    // function (e.g. b_process_manager_check).
    virtual B_FUNC
    wait_and_notify(
            B_ProcessManager *,
            std::chrono::nanoseconds timeout,
            B_OUT bool *timed_out,
            B_ErrorHandler const *) = 0;

    // Waits for a child event, then calls the appropriate
    // function (e.g. b_process_manager_check).  Keeps doing
    // this until a timeout or until callback returns true.
    template<typename TCallback>
    B_FUNC
    wait_and_notify_until(
            B_ProcessManager *manager,
            std::chrono::nanoseconds timeout,
            B_OUT bool *timed_out,
            TCallback const &callback,
            B_ErrorHandler const *eh) {
        while (!callback()) {
            // FIXME(strager): The timeout should decrease
            // after every call.
            if (!this->wait_and_notify(
                    manager, timeout, timed_out, eh)) {
                return false;
            }
            if (*timed_out) {
                break;
            }
        }
        return true;
    }
};

#if defined(B_CONFIG_POSIX_SIGNALS) \
    && !defined(B_CONFIG_BROKEN_PSELECT)
class PselectInterruptTester : public Tester {
public:
    class Factory : public Tester::Factory {
    public:
        std::unique_ptr<Tester>
        create_tester() const override {
            return std::unique_ptr<Tester>(
                new PselectInterruptTester());
        }

        const char *
        get_type_string() const override {
            return "pselect interrupt";
        }
    };

    PselectInterruptTester() {
        this->register_process_id = register_process_id_;
        this->unregister_process_id
            = unregister_process_id_;

        // Enable EINTR if SIGCHLD is received.
        struct sigaction action;
        action.sa_flags = SA_SIGINFO | SA_NOCLDSTOP;
        action.sa_sigaction = handle_sigchld_;
        EXPECT_EQ(0, sigemptyset(&action.sa_mask));
        EXPECT_EQ(0, sigaction(
            SIGCHLD, &action, &old_sigchld_action));

        // Block SIGCHLD in case SIGCHLD is received before
        // wait_and_notify is called.
        sigset_t old_mask;
        EXPECT_EQ(0, sigprocmask(0, nullptr, &old_mask));
        if (sigismember(&old_mask, SIGCHLD)) {
            // SIGCHLD is already blocked.
            this->blocked_sigchld = false;
        } else {
            // SIGCHLD needs to be blocked.
            this->blocked_sigchld = true;

            sigset_t mask;
            EXPECT_EQ(0, sigemptyset(&mask));
            EXPECT_EQ(0, sigaddset(&mask, SIGCHLD));
            EXPECT_EQ(0, sigprocmask(
                SIG_BLOCK, &mask, nullptr));
        }
    }

    ~PselectInterruptTester() {
        if (this->blocked_sigchld) {
            sigset_t mask;
            EXPECT_EQ(0, sigemptyset(&mask));
            EXPECT_EQ(0, sigaddset(&mask, SIGCHLD));
            EXPECT_EQ(0, sigprocmask(
                SIG_UNBLOCK, &mask, nullptr));
        }
        EXPECT_EQ(0, sigaction(
            SIGCHLD, &old_sigchld_action, nullptr));
    }

    B_FUNC
    wait_and_notify(
            B_ProcessManager *manager,
            std::chrono::nanoseconds timeout_ns,
            bool *timed_out,
            B_ErrorHandler const *eh) override {
        // Unblock SIGCHLD during select.  In other words,
        // allow EINTR from SIGCHLD.
        sigset_t mask;
        EXPECT_EQ(0, sigprocmask(0, nullptr, &mask));
        EXPECT_EQ(0, sigdelset(&mask, SIGCHLD));

        struct timespec timeout
            = duration_to_timespec_(timeout_ns);
        int rc = pselect(
            0, nullptr, nullptr, nullptr, &timeout, &mask);
        if (rc == -1) {
            EXPECT_EQ(EINTR, errno) << strerror(errno);
            if (errno != EINTR) {
                return false;
            }
            if (!b_process_manager_check(manager, eh)) {
                return false;
            }
            *timed_out = false;
        } else {
            EXPECT_EQ(0, rc);
            *timed_out = true;
        }
        return true;
    }

private:
    bool blocked_sigchld;
    struct sigaction old_sigchld_action;

    static B_FUNC
    register_process_id_(
            struct B_ProcessControllerDelegate *,
            B_ProcessID,
            struct B_ErrorHandler const *) {
        // Do nothing.
        return true;
    }

    static B_FUNC
    unregister_process_id_(
            struct B_ProcessControllerDelegate *,
            B_ProcessID,
            struct B_ErrorHandler const *) {
        // Do nothing.
        return true;
    }

    static void
    handle_sigchld_(
            int,
            siginfo_t *,
            void *) {
        int old_errno = errno;

        // Print a message useful for debugging.
        static char const message[]
            = "[[Received SIGCHLD]]\n";
        (void) write(2, message, sizeof(message) - 1);

        errno = old_errno;
    }
};
#endif

#if defined(B_CONFIG_KQUEUE)
class KqueueTester : public Tester {
public:
    class Factory : public Tester::Factory {
    public:
        std::unique_ptr<Tester>
        create_tester() const override {
            return std::unique_ptr<Tester>(
                new KqueueTester());
        }

        const char *
        get_type_string() const override {
            return "kqueue";
        }
    };

    KqueueTester() {
        this->register_process_id = register_process_id_;
        this->unregister_process_id
            = unregister_process_id_;

        this->kqueue = ::kqueue();
        EXPECT_NE(-1, this->kqueue) << strerror(errno);
    }

    ~KqueueTester() {
        EXPECT_EQ(0, close(this->kqueue))
            << strerror(errno);
    }

    B_FUNC
    wait_and_notify(
            B_ProcessManager *manager,
            std::chrono::nanoseconds timeout_ns,
            bool *timed_out,
            B_ErrorHandler const *eh) override {
        struct timespec timeout
            = duration_to_timespec_(timeout_ns);
        struct kevent events[10];
        int rc = kevent(
            this->kqueue,
            nullptr,
            0,
            events,
            sizeof(events) / sizeof(*events),
            &timeout);
        if (rc == -1) {
            EXPECT_EQ(EINTR, errno) << strerror(errno);
            if (errno != EINTR) {
                return false;
            }
            *timed_out = false;
        } else if (rc == 0) {
            EXPECT_EQ(0, rc);
            *timed_out = true;
        } else {
            bool notify = false;
            for (int i = 0; i < rc; ++i) {
                EXPECT_EQ(EVFILT_PROC, events[i].filter);

                if (events[i].fflags & NOTE_EXIT) {
                    // FIXME(strager): Should we pass down
                    // events[i].data instead?
                    notify = true;
                }
            }
            if (notify) {
                if (!b_process_manager_check(manager, eh)) {
                    return false;
                }
            }
        }
        return true;
    }

private:
    int kqueue;

    static B_FUNC
    register_process_id_(
            struct B_ProcessControllerDelegate *delegate,
            B_ProcessID process_id,
            struct B_ErrorHandler const *) {
        auto self = static_cast<KqueueTester *>(delegate);
        struct kevent change;
        EV_SET(
            &change,
            static_cast<pid_t>(process_id),
            EVFILT_PROC,
            EV_ADD,
            NOTE_EXIT,
            0,
            nullptr);
        EXPECT_EQ(0, kevent(
            self->kqueue, &change, 1, nullptr, 0, nullptr));
        return true;
    }

    static B_FUNC
    unregister_process_id_(
            struct B_ProcessControllerDelegate *delegate,
            B_ProcessID process_id,
            struct B_ErrorHandler const *) {
        auto self = static_cast<KqueueTester *>(delegate);
        struct kevent change;
        EV_SET(
            &change,
            static_cast<pid_t>(process_id),
            EVFILT_PROC,
            EV_DELETE,
            NOTE_EXIT,
            0,
            nullptr);
        EXPECT_EQ(0, kevent(
            self->kqueue, &change, 1, nullptr, 0, nullptr));
        return true;
    }

    static void
    handle_sigchld_(
            int,
            siginfo_t *,
            void *) {
        int old_errno = errno;

        // Print a message useful for debugging.
        static char const message[]
            = "[[Received SIGCHLD]]\n";
        (void) write(2, message, sizeof(message) - 1);

        errno = old_errno;
    }
};
#endif

std::ostream &operator<<(
        std::ostream &stream,
        Testers::Tester::Factory *tester_factory) {
    if (tester_factory) {
        stream << tester_factory->get_type_string();
    } else {
        stream << "(null)";
    }
    return stream;
}

Testers::Tester::Factory *tester_factories[] = {
#if defined(B_CONFIG_POSIX_SIGNALS) \
    && !defined(B_CONFIG_BROKEN_PSELECT)
    new Testers::PselectInterruptTester::Factory(),
#endif
#if defined(B_CONFIG_KQUEUE)
    new Testers::KqueueTester::Factory(),
#endif
};

}

class TestProcess : public ::testing::TestWithParam<
        Testers::Tester::Factory *> {
public:
    std::unique_ptr<Testers::Tester>
    create_tester() const {
        return this->GetParam()->create_tester();
    }
};

INSTANTIATE_TEST_CASE_P(
    ,  // No prefix.
    TestProcess,
    ::testing::ValuesIn(Testers::tester_factories));

TEST_P(TestProcess, SanityCheck) {
    // Make sure the tester isn't doing something insane.
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));
    for (size_t i = 0; i < 10; ++i) {
        // FIXME(strager): libstdc++ bug?
#if 0
        SCOPED_TRACE((std::ostringstream() << "Iteration "
            << i).str());
#endif

        bool timed_out;
        EXPECT_TRUE(tester->wait_and_notify(
            manager,
            std::chrono::nanoseconds::zero(),
            &timed_out,
            eh));
    }

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}

TEST_P(TestProcess, TrueReturnsPromptly) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(manager, {"true"}, &exited, &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager,
        std::chrono::seconds(1),
        &timed_out,
        [&exited]() {
            return exited;
        },
        eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
    EXPECT_EQ(0, exit_status.code.exit_code);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}

TEST_P(TestProcess, FalseReturnsPromptly) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(manager, {"false"}, &exited, &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager,
        std::chrono::seconds(1),
        &timed_out,
        [&exited]() {
            return exited;
        },
        eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
    EXPECT_NE(0, exit_status.code.exit_code);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}

#if defined(B_CONFIG_POSIX_SIGNALS)
TEST_P(TestProcess, KillSIGTERM) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(
        manager,
        {"sh", "-c", "kill -TERM $$"},
        &exited,
        &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager,
        std::chrono::seconds(1),
        &timed_out,
        [&exited]() {
            return exited;
        },
        eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(
        B_PROCESS_EXIT_STATUS_SIGNAL, exit_status.type);
    EXPECT_EQ(SIGTERM, exit_status.signal.signal_number);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}
#endif

TEST_P(TestProcess, MultipleTruesInParallel) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));

    B_ProcessController *controller;
    ASSERT_TRUE(b_process_manager_get_controller(
        manager, &controller, eh));

    size_t const process_count = 20;
    size_t exited_count = 0;
    std::vector<B_ProcessExitStatus> exit_statuses(
        process_count);

    for (B_ProcessExitStatus &exit_status : exit_statuses) {
        char const *const args[] = { "true", nullptr };
        ASSERT_TRUE(b_process_controller_exec_basic(
            controller,
            args,
            [&exited_count, &exit_status](
                    B_ProcessExitStatus const *status,
                    B_ErrorHandler const *) -> bool {
                exited_count += 1;
                exit_status = *status;
                return true;
            },
            eh));
    }

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager,
        std::chrono::seconds(1),
        &timed_out,
        [&exited_count, process_count]() {
            return exited_count >= process_count;
        },
        eh));
    EXPECT_FALSE(timed_out);
    EXPECT_EQ(process_count, exited_count);
    for (B_ProcessExitStatus status : exit_statuses) {
        EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, status.type);
        EXPECT_EQ(0, status.code.exit_code);
    }

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}

namespace {

struct ProcessTree {
private:
    B_ProcessController *controller;

    B_ProcessExitStatus expected_exit_status;

    // Child program arguments.
    std::vector<char const *> program_args;

    // Run these processes when the current process
    // completes.
    std::vector<ProcessTree> next_processes;

    B_ProcessExitStatus exit_status;
    bool exited;

public:
    ProcessTree(
            B_ProcessController *controller,
            B_ProcessExitStatus expected_exit_status,
            std::vector<char const *> program_args,
            std::vector<ProcessTree> next_processes) :
            controller(controller),
            expected_exit_status(expected_exit_status),
            program_args(program_args),
            next_processes(next_processes),
            exited(false) {
    }

    B_FUNC
    exec(
            B_ErrorHandler const *eh) {
        std::vector<char const *> args = this->program_args;
        if (args.empty() || args.back()) {
            args.push_back(nullptr);
        }
        return b_process_controller_exec_basic(
            this->controller, args.data(), [this](
                    B_ProcessExitStatus const *status,
                    B_ErrorHandler const *eh) {
                return this->exited_callback(status, eh);
            }, eh);
    }

    bool
    fully_exited() const {
        if (!this->exited) {
            return false;
        }
        for (ProcessTree const &process
                : this->next_processes) {
            if (!process.fully_exited()) {
                return false;
            }
        }
        return true;
    }

private:
    B_FUNC
    exited_callback(
            B_ProcessExitStatus const *status,
            B_ErrorHandler const *eh) {
        EXPECT_FALSE(this->exited);
        this->exited = true;
        this->exit_status = *status;
        EXPECT_EQ(
            this->expected_exit_status, this->exit_status);

        for (ProcessTree &process : this->next_processes) {
            EXPECT_TRUE(process.exec(eh));
        }

        return true;
    }
};

}

TEST_P(TestProcess, DeeplyNestedProcessTree) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));

    B_ProcessController *controller;
    ASSERT_TRUE(b_process_manager_get_controller(
        manager, &controller, eh));

    B_ProcessExitStatus success;
    success.type = B_PROCESS_EXIT_STATUS_CODE;
    success.code.exit_code = 0;

    ProcessTree process_tree(
        controller, success, {"true"}, {});
    for (size_t i = 0; i < 50; ++i) {
        process_tree = ProcessTree(
            controller,
            success,
            {"true"},
            {std::move(process_tree)});
    }

    ASSERT_TRUE(process_tree.exec(eh));

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager,
        std::chrono::seconds(1),
        &timed_out,
        [&process_tree]() {
            return process_tree.fully_exited();
        },
        eh));
    EXPECT_FALSE(timed_out);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}

TEST_P(TestProcess, VeryBranchyProcessTree) {
    B_ErrorHandler const *eh = nullptr;
    auto tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester.get(), &manager, eh));

    B_ProcessController *controller;
    ASSERT_TRUE(b_process_manager_get_controller(
        manager, &controller, eh));

    B_ProcessExitStatus success;
    success.type = B_PROCESS_EXIT_STATUS_CODE;
    success.code.exit_code = 0;

    ProcessTree leaf_process_tree(
        controller, success, {"true"}, {});
    ProcessTree branch_process_tree(
        controller,
        success,
        {"true"},
        std::vector<ProcessTree>(10, leaf_process_tree));
    ProcessTree process_tree(
        controller,
        success,
        {"true"},
        std::vector<ProcessTree>(10, branch_process_tree));
    ASSERT_TRUE(process_tree.exec(eh));

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager,
        std::chrono::seconds(1),
        &timed_out,
        [&process_tree]() {
            return process_tree.fully_exited();
        },
        eh));
    EXPECT_FALSE(timed_out);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
}
