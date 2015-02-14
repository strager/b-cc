#include "Mocks.h"
#include "Util.h"

#include <B/Assert.h>
#include <B/Config.h>
#include <B/Private/Thread.h>
#include <B/Process.h>

#include <errno.h>
#include <gtest/gtest.h>
#include <list>
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

#if defined(__linux__)
// TestProcess::ExecutableNotFound fails on Linux
// https://github.com/strager/b-cc/issues/25
# define B_BUG_25
#endif

#if defined(B_CONFIG_POSIX_SIGNALS)
class SigactionHolder {
public:
    // Non-copyable.
    SigactionHolder(
            SigactionHolder const &) = delete;
    SigactionHolder &
    operator=(
            SigactionHolder const &) = delete;

    SigactionHolder() :
            armed(false) {
    }

    ~SigactionHolder() {
        this->disarm();
    }

    void
    arm(
            int signal_number,
            struct sigaction new_action) {
        this->disarm();

        int rc;
        EXPECT_EQ(0, (rc = sigaction(
            signal_number, &new_action, &this->action)))
            << strerror(errno);
        if (rc == 0) {
            this->signal_number = signal_number;
            this->armed = true;
        }
    }

    void
    disarm() {
        if (!this->armed) {
            return;
        }

        int rc;
        EXPECT_EQ(0, (rc = sigaction(
            this->signal_number, &this->action, nullptr)))
            << strerror(errno);
        if (rc == 0) {
            this->armed = false;
        }
    }

private:
    bool armed;
    int signal_number;
    struct sigaction action;
};
#endif

typedef uint64_t Nanoseconds;
enum {
    NS_PER_S = 1000 * 1000 * 1000,
};

static struct timespec
nanoseconds_to_timespec_(
        Nanoseconds duration) {
    struct timespec time;
    time.tv_sec = duration / NS_PER_S;
    time.tv_nsec = duration % NS_PER_S;
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

#if defined(B_CONFIG_POSIX_SIGNALS)
static std::string
test_process_child_path_() {
    return dirname(get_executable_path())
        + "/TestProcessChild";
}
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
        virtual Tester *
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
            Nanoseconds timeout,
            B_OUT bool *timed_out,
            B_ErrorHandler const *) = 0;

    // Waits for a child event, then calls the appropriate
    // function (e.g. b_process_manager_check).  Keeps doing
    // this until a timeout or until callback returns true.
    template<typename TCallback>
    B_FUNC
    wait_and_notify_until(
            B_ProcessManager *manager,
            Nanoseconds timeout,
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
        Tester *
        create_tester() const override {
            return new PselectInterruptTester();
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
        this->sigaction_holder.arm(SIGCHLD, action);

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
    }

    B_FUNC
    wait_and_notify(
            B_ProcessManager *manager,
            Nanoseconds timeout_ns,
            bool *timed_out,
            B_ErrorHandler const *eh) override {
        // Unblock SIGCHLD during select.  In other words,
        // allow EINTR from SIGCHLD.
        sigset_t mask;
        EXPECT_EQ(0, sigprocmask(0, nullptr, &mask));
        EXPECT_EQ(0, sigdelset(&mask, SIGCHLD));

        struct timespec timeout
            = nanoseconds_to_timespec_(timeout_ns);
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
    SigactionHolder sigaction_holder;

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
        Tester *
        create_tester() const override {
            return new KqueueTester();
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
            Nanoseconds timeout_ns,
            bool *timed_out,
            B_ErrorHandler const *eh) override {
        struct timespec timeout
            = nanoseconds_to_timespec_(timeout_ns);
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

using Testers::Tester;

class TestProcess : public ::testing::TestWithParam<
        Tester::Factory *> {
public:
    Tester *
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
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));
    for (size_t i = 0; i < 10; ++i) {
        bool timed_out;
        EXPECT_TRUE(tester->wait_and_notify(
            manager, 0, &timed_out, eh));
    }

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

TEST_P(TestProcess, TrueReturnsPromptly) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(manager, {"true"}, &exited, &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager, 1 * NS_PER_S, &timed_out, [&exited]() {
            return exited;
        }, eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
    EXPECT_EQ(0, exit_status.code.exit_code);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

TEST_P(TestProcess, FalseReturnsPromptly) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(manager, {"false"}, &exited, &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager, 1 * NS_PER_S, &timed_out, [&exited]() {
            return exited;
        }, eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
    EXPECT_NE(0, exit_status.code.exit_code);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

#if defined(B_BUG_25)
TEST_P(TestProcess, DISABLED_ExecutableNotFound) {
#else
TEST_P(TestProcess, ExecutableNotFound) {
#endif
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));
    B_ProcessController *controller;
    ASSERT_TRUE(b_process_manager_get_controller(
        manager, &controller, eh));

    char const *args[] = {
        "does_not_exist",
        nullptr,
    };
    MockErrorHandler mock_eh;
    EXPECT_CALL(mock_eh, handle_error(B_Error{ENOENT}))
        .WillOnce(Return(B_ERROR_ABORT));
    EXPECT_FALSE(b_process_controller_exec_basic(
        controller,
        args,
        [](
                B_ProcessExitStatus const *,
                void *,
                B_ErrorHandler const *) -> bool {
            ADD_FAILURE();
            return true;
        },
        nullptr,
        &mock_eh));

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

#if defined(B_CONFIG_POSIX_SIGNALS)
TEST_P(TestProcess, KillSIGTERM) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(
        manager,
        {"sh", "-c", "kill -TERM $$"},
        &exited,
        &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager, 1 * NS_PER_S, &timed_out, [&exited]() {
            return exited;
        }, eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(
        B_PROCESS_EXIT_STATUS_SIGNAL, exit_status.type);
    EXPECT_EQ(SIGTERM, exit_status.signal.signal_number);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}
#endif

TEST_P(TestProcess, MultipleTruesInParallel) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

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
        1 * NS_PER_S,
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
    delete tester;
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
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

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
        1 * NS_PER_S,
        &timed_out,
        [&process_tree]() {
            return process_tree.fully_exited();
        },
        eh));
    EXPECT_FALSE(timed_out);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

TEST_P(TestProcess, VeryBranchyProcessTree) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

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
        1 * NS_PER_S,
        &timed_out,
        [&process_tree]() {
            return process_tree.fully_exited();
        },
        eh));
    EXPECT_FALSE(timed_out);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

#if defined(B_CONFIG_POSIX_SIGNALS)
TEST_P(TestProcess, BasicExecChildHasCleanSignalMask) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(
        manager,
        {test_process_child_path_().c_str(),
            "BasicExecChildHasCleanSignalMask"},
        &exited,
        &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager, 1 * NS_PER_S, &timed_out, [&exited]() {
            return exited;
        }, eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
    EXPECT_EQ(0, exit_status.code.exit_code);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}

TEST_P(TestProcess, BasicExecChildDoesNotInheritSigaction) {
    B_ErrorHandler const *eh = nullptr;
    Tester *tester = this->create_tester();

    // Ignore SIGALRM.  The child process should have
    // SIG_DFL for SIGALRM.
    struct sigaction action;
    action.sa_flags = 0;
    action.sa_handler = SIG_IGN;
    EXPECT_EQ(0, sigemptyset(&action.sa_mask));
    SigactionHolder holder;
    holder.arm(SIGALRM, action);

    B_ProcessManager *manager;
    ASSERT_TRUE(b_process_manager_allocate(
        tester, &manager, eh));

    bool exited = false;
    B_ProcessExitStatus exit_status;
    exec_basic_(
        manager,
        {test_process_child_path_().c_str(),
            "BasicExecChildDoesNotInheritSigaction"},
        &exited,
        &exit_status);

    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify_until(
        manager, 1 * NS_PER_S, &timed_out, [&exited]() {
            return exited;
        }, eh));
    EXPECT_FALSE(timed_out);
    EXPECT_TRUE(exited);
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
    EXPECT_EQ(0, exit_status.code.exit_code);

    EXPECT_TRUE(b_process_manager_deallocate(manager, eh));
    delete tester;
}
#endif
