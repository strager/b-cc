#include "Executable.h"

#include <B/Error.h>
#include <B/Private/Config.h>
#include <B/Process.h>

#include <errno.h>
#include <gtest/gtest.h>
#include <list>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#if B_CONFIG_POSIX_SIGNALS
# include <signal.h>
# include <unistd.h>
#endif

#if B_CONFIG_KQUEUE
# include <sys/event.h>
#endif

#if defined(__linux__)
// TestProcess::ExecutableNotFound fails on Linux
// https://github.com/strager/b-cc/issues/25
# define B_BUG_25
#endif

#if __cplusplus < 201100L
# define override
#endif

#if B_CONFIG_POSIX_SIGNALS
namespace {

class B_SigactionHolder_ {
public:
  // Non-copyable.
#if __cplusplus >= 201100L
  B_SigactionHolder_(
      B_SigactionHolder_ const &) = delete;
  B_SigactionHolder_ &
  operator=(
      B_SigactionHolder_ const &) = delete;
#endif

  B_SigactionHolder_() :
      armed(false) {
  }

  ~B_SigactionHolder_() {
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
      this->signal_number, &this->action, NULL)))
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

}
#endif

typedef uint64_t B_Nanoseconds_;
enum {
  B_NS_PER_S_ = 1000 * 1000 * 1000,
};

static struct timespec
b_nanoseconds_to_timespec_(
    B_Nanoseconds_ duration) {
  struct timespec time;
  time.tv_sec = duration / B_NS_PER_S_;
  time.tv_nsec = duration % B_NS_PER_S_;
  return time;
}

struct B_ExecBasicClosure_ {
  size_t *exit_count;
  struct B_ProcessExitStatus *exit_status;
};

static B_FUNC bool
b_exec_basic_callback_(
    struct B_ProcessExitStatus const *in_exit_status,
    void *opaque,
    struct B_Error *) {
  B_ExecBasicClosure_ *closure
    = static_cast<B_ExecBasicClosure_ *>(opaque);
  *closure->exit_count += 1;
  *closure->exit_status = *in_exit_status;
  delete closure;
  return true;
}

static void
b_exec_basic_(
    B_BORROW struct B_ProcessManager *manager,
    B_BORROW std::vector<char const *> args,
    // Asynchronously incremented.
    B_OUT size_t *exit_count,
    // Asynchronously set.
    B_OUT struct B_ProcessExitStatus *exit_status) {
  struct B_Error e;
  args.push_back(NULL);
  B_ProcessController *controller;
  ASSERT_TRUE(b_process_manager_get_controller(
    manager, &controller, &e));
  B_ExecBasicClosure_ *closure = new B_ExecBasicClosure_;
  closure->exit_count = exit_count;
  closure->exit_status = exit_status;
  ASSERT_TRUE(b_process_controller_exec_basic(
    controller,
    args.data(),
    b_exec_basic_callback_,
    closure,
    &e));
}

template<typename T>
static std::vector<T>
b_vec_() {
  return std::vector<T>();
}

template<typename T>
static std::vector<T>
b_vec_(
    T a) {
  std::vector<T> v;
  v.push_back(a);
  return v;
}

template<typename T>
static std::vector<T>
b_vec_(
    T a,
    T b) {
  std::vector<T> v;
  v.push_back(a);
  v.push_back(b);
  return v;
}

template<typename T>
static std::vector<T>
b_vec_(
    T a,
    T b,
    T c) {
  std::vector<T> v;
  v.push_back(a);
  v.push_back(b);
  v.push_back(c);
  return v;
}

#if B_CONFIG_POSIX_SIGNALS
static char const *
b_test_process_child_path_() {
  static std::string path = dirname(get_executable_path())
    + "/TestProcessChild";
  return path.c_str();
}
#endif

static B_FUNC bool
b_register_process_id_dummy_(
    struct B_ProcessControllerDelegate *,
    B_ProcessID,
    struct B_Error *) {
  ADD_FAILURE();
  return false;
}

static B_FUNC bool
b_register_process_handle_dummy_(
    struct B_ProcessControllerDelegate *,
    void *,
    struct B_Error *) {
  ADD_FAILURE();
  return false;
}

TEST(TestProcessVanilla, ValidDelegateImplementations) {
  struct B_Error e;

  // register_process_id/unregister_process_id
  B_FUNC bool (*r_id)(
      struct B_ProcessControllerDelegate *,
      B_ProcessID,
      struct B_Error *) = b_register_process_id_dummy_;
  // register_process_handle/unregister_process_handle
  B_FUNC bool (*r_h)(
      struct B_ProcessControllerDelegate *,
      void *,
      struct B_Error *) = b_register_process_handle_dummy_;

  struct TestCase {
    B_ProcessControllerDelegate delegate;
    bool should_succeed;
  };
  TestCase test_cases[] = {
    { { NULL, NULL, NULL, NULL }, false },
    { { r_id, r_id, NULL, NULL }, true  },
    { { NULL, NULL, r_h,  r_h  }, true  },
    { { r_id, r_id, r_h,  r_h  }, true  },

    { { r_id, NULL, NULL, NULL }, false },
    { { r_id, NULL, r_h,  r_h  }, false },
    { { NULL, r_id, NULL, NULL }, false },
    { { NULL, r_id, r_h,  r_h  }, false },

    { { NULL, NULL, r_h,  NULL }, false },
    { { NULL, NULL, NULL, r_h  }, false },
    { { r_id, r_id, r_h,  NULL }, false },
    { { r_id, r_id, NULL, r_h  }, false },
  };

  for (
      TestCase *test_case = test_cases;
      test_case != test_cases
        + (sizeof(test_cases) / sizeof(*test_cases));
      ++test_case) {
    if (test_case->should_succeed) {
      struct B_ProcessManager *manager;
      ASSERT_TRUE(b_process_manager_allocate(
        &test_case->delegate, &manager, &e));
      EXPECT_TRUE(b_process_manager_deallocate(
        manager, &e));
    } else {
      struct B_ProcessManager *manager;
      EXPECT_FALSE(b_process_manager_allocate(
        &test_case->delegate, &manager, &e));
      EXPECT_EQ(EINVAL, e.posix_error);
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
    this->register_process_id = NULL;
    this->unregister_process_id = NULL;
    this->register_process_handle = NULL;
    this->unregister_process_handle = NULL;
  }

  virtual ~Tester() {
  }

  // Waits for a child event, then calls the appropriate
  // function (e.g. b_process_manager_check).
  virtual B_WUR B_FUNC bool
  wait_and_notify(
      B_ProcessManager *,
      B_Nanoseconds_ timeout,
      B_OUT bool *timed_out,
      B_OUT struct B_Error *) = 0;

  // Waits for a child event, then calls the appropriate
  // function (e.g. b_process_manager_check).  Keeps doing
  // this until a timeout or until callback returns true.
  template<typename TCallback>
  B_WUR B_FUNC bool
  wait_and_notify_until(
      B_ProcessManager *manager,
      B_Nanoseconds_ timeout,
      B_OUT bool *timed_out,
      TCallback const &callback,
      B_OUT struct B_Error *e) {
    while (!callback()) {
      // FIXME(strager): The timeout should decrease
      // after every call.
      if (!this->wait_and_notify(
          manager, timeout, timed_out, e)) {
        return false;
      }
      if (*timed_out) {
        break;
      }
    }
    return true;
  }
};

#if B_CONFIG_POSIX_SIGNALS && !B_CONFIG_BROKEN_PSELECT
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
    EXPECT_EQ(0, sigprocmask(0, NULL, &old_mask));
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
        SIG_BLOCK, &mask, NULL));
    }
  }

  ~PselectInterruptTester() {
    if (this->blocked_sigchld) {
      sigset_t mask;
      EXPECT_EQ(0, sigemptyset(&mask));
      EXPECT_EQ(0, sigaddset(&mask, SIGCHLD));
      EXPECT_EQ(0, sigprocmask(
        SIG_UNBLOCK, &mask, NULL));
    }
  }

  B_WUR B_FUNC bool
  wait_and_notify(
      B_ProcessManager *manager,
      B_Nanoseconds_ timeout_ns,
      bool *timed_out,
      B_OUT struct B_Error *e) override {
    // Unblock SIGCHLD during select.  In other words,
    // allow EINTR from SIGCHLD.
    sigset_t mask;
    EXPECT_EQ(0, sigprocmask(0, NULL, &mask));
    EXPECT_EQ(0, sigdelset(&mask, SIGCHLD));

    struct timespec timeout
      = b_nanoseconds_to_timespec_(timeout_ns);
    int rc = pselect(
      0, NULL, NULL, NULL, &timeout, &mask);
    if (rc == -1) {
      EXPECT_EQ(EINTR, errno) << strerror(errno);
      if (errno != EINTR) {
        return false;
      }
      if (!b_process_manager_check(manager, e)) {
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
  B_SigactionHolder_ sigaction_holder;

  static B_WUR B_FUNC bool
  register_process_id_(
      struct B_ProcessControllerDelegate *,
      B_ProcessID,
      struct B_Error *) {
    // Do nothing.
    return true;
  }

  static B_WUR B_FUNC bool
  unregister_process_id_(
      struct B_ProcessControllerDelegate *,
      B_ProcessID,
      struct B_Error *) {
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

#if B_CONFIG_KQUEUE
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

  B_WUR B_FUNC bool
  wait_and_notify(
      B_ProcessManager *manager,
      B_Nanoseconds_ timeout_ns,
      bool *timed_out,
      B_OUT struct B_Error *e) override {
    struct timespec timeout
      = b_nanoseconds_to_timespec_(timeout_ns);
    struct kevent events[10];
    int rc = kevent(
      this->kqueue,
      NULL,
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
        if (!b_process_manager_check(manager, e)) {
          return false;
        }
      }
      *timed_out = false;
    }
    return true;
  }

private:
  int kqueue;

  static B_WUR B_FUNC bool
  register_process_id_(
      struct B_ProcessControllerDelegate *delegate,
      B_ProcessID process_id,
      struct B_Error *) {
    KqueueTester *self
      = static_cast<KqueueTester *>(delegate);
    struct kevent change;
    EV_SET(
      &change,
      static_cast<pid_t>(process_id),
      EVFILT_PROC,
      EV_ADD,
      NOTE_EXIT,
      0,
      NULL);
    EXPECT_EQ(0, kevent(
      self->kqueue, &change, 1, NULL, 0, NULL));
    return true;
  }

  static B_WUR B_FUNC bool
  unregister_process_id_(
      struct B_ProcessControllerDelegate *delegate,
      B_ProcessID process_id,
      struct B_Error *) {
    KqueueTester *self
      = static_cast<KqueueTester *>(delegate);
    struct kevent change;
    EV_SET(
      &change,
      static_cast<pid_t>(process_id),
      EVFILT_PROC,
      EV_DELETE,
      NOTE_EXIT,
      0,
      NULL);
    EXPECT_EQ(0, kevent(
      self->kqueue, &change, 1, NULL, 0, NULL));
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
#if B_CONFIG_POSIX_SIGNALS && !B_CONFIG_BROKEN_PSELECT
  new Testers::PselectInterruptTester::Factory(),
#endif
#if B_CONFIG_KQUEUE
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
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));
  for (size_t i = 0; i < 10; ++i) {
    bool timed_out;
    EXPECT_TRUE(tester->wait_and_notify(
      manager, 0, &timed_out, &e));
  }

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

struct B_WaitUntilExitedFunctor_ {
  B_WaitUntilExitedFunctor_(
      size_t *exit_count,
      size_t expected_exit_count = 1) :
      exit_count(exit_count),
      expected_exit_count(expected_exit_count) {
  }

  size_t *exit_count;
  size_t expected_exit_count;

  bool
  operator()() const {
    return *this->exit_count >= this->expected_exit_count;
  }
};

TEST_P(TestProcess, TrueReturnsPromptly) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  size_t exit_count = 0;
  B_ProcessExitStatus exit_status;
  b_exec_basic_(
    manager, b_vec_("true"), &exit_count, &exit_status);

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilExitedFunctor_(&exit_count),
    &e));
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(1U, exit_count);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_EQ(0, exit_status.code.exit_code);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

TEST_P(TestProcess, FalseReturnsPromptly) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  size_t exit_count = 0;
  B_ProcessExitStatus exit_status;
  b_exec_basic_(
    manager, b_vec_("false"), &exit_count, &exit_status);

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilExitedFunctor_(&exit_count),
    &e));
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(1U, exit_count);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_NE(0, exit_status.code.exit_code);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

static B_FUNC bool
b_exec_callback_add_failure_(
    struct B_ProcessExitStatus const *,
    void *,
    B_OUT struct B_Error *) {
  ADD_FAILURE();
  return true;
}

#if defined(B_BUG_25)
TEST_P(TestProcess, DISABLED_ExecutableNotFound) {
#else
TEST_P(TestProcess, ExecutableNotFound) {
#endif
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));
  B_ProcessController *controller;
  ASSERT_TRUE(b_process_manager_get_controller(
    manager, &controller, &e));

  char const *args[] = {"does_not_exist", NULL};
  EXPECT_FALSE(b_process_controller_exec_basic(
    controller,
    args,
    b_exec_callback_add_failure_,
    NULL,
    &e));
  EXPECT_EQ(ENOENT, e.posix_error);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

#if B_CONFIG_POSIX_SIGNALS
TEST_P(TestProcess, KillSIGTERM) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  size_t exit_count = 0;
  B_ProcessExitStatus exit_status;
  b_exec_basic_(
    manager,
    b_vec_("sh", "-c", "kill -TERM $$"),
    &exit_count,
    &exit_status);

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilExitedFunctor_(&exit_count),
    &e));
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(1U, exit_count);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_SIGNAL, exit_status.type);
  EXPECT_EQ(SIGTERM, exit_status.signal.signal_number);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}
#endif

TEST_P(TestProcess, MultipleTruesInParallel) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  B_ProcessController *controller;
  ASSERT_TRUE(b_process_manager_get_controller(
    manager, &controller, &e));

  size_t const process_count = 20;
  size_t exit_count = 0;
  std::vector<B_ProcessExitStatus> exit_statuses(
    process_count);

  for (
      std::vector<B_ProcessExitStatus>::iterator i
        = exit_statuses.begin();
      i != exit_statuses.end();
      ++i) {
    b_exec_basic_(
      manager, b_vec_("true"), &exit_count, &*i);
  }

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilExitedFunctor_(&exit_count, process_count),
    &e));
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(process_count, exit_count);
  for (
      std::vector<B_ProcessExitStatus>::iterator i
        = exit_statuses.begin();
      i != exit_statuses.end();
      ++i) {
    EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, i->type);
    EXPECT_EQ(0, i->code.exit_code);
  }

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

namespace {

static B_ProcessExitCallback
b_process_tree_process_exited_;

struct B_ProcessTree_ {
private:
  B_ProcessController *controller;

  B_ProcessExitStatus expected_exit_status;

  // Child program arguments.
  std::vector<char const *> program_args;

  // Run these processes when the current process
  // completes.
  std::vector<B_ProcessTree_> next_processes;

  B_ProcessExitStatus exit_status;
  bool exited;

public:
  B_ProcessTree_(
      B_ProcessController *controller,
      B_ProcessExitStatus expected_exit_status,
      std::vector<char const *> program_args,
      std::vector<B_ProcessTree_> next_processes) :
      controller(controller),
      expected_exit_status(expected_exit_status),
      program_args(program_args),
      next_processes(next_processes),
      exited(false) {
  }

  B_WUR B_FUNC bool
  exec(
      B_OUT struct B_Error *e) {
    std::vector<char const *> args = this->program_args;
    if (args.empty() || args.back()) {
      args.push_back(NULL);
    }
    return b_process_controller_exec_basic(
      this->controller,
      args.data(),
      b_process_tree_process_exited_,
      this,
      e);
  }

  bool
  fully_exited() const {
    if (!this->exited) {
      return false;
    }
    for (
        std::vector<B_ProcessTree_>::const_iterator i
          = this->next_processes.begin();
        i != this->next_processes.end();
        ++i) {
      if (!i->fully_exited()) {
        return false;
      }
    }
    return true;
  }

private:
  B_WUR B_FUNC bool
  exited_callback(
      B_ProcessExitStatus const *status,
      B_OUT struct B_Error *e) {
    EXPECT_FALSE(this->exited);
    this->exited = true;
    this->exit_status = *status;
    EXPECT_EQ(
      this->expected_exit_status, this->exit_status);

    for (
        std::vector<B_ProcessTree_>::iterator i
          = this->next_processes.begin();
        i != this->next_processes.end();
        ++i) {
      EXPECT_TRUE(i->exec(e));
    }

    return true;
  }

  friend B_ProcessExitCallback
  b_process_tree_process_exited_;
};

static B_FUNC bool
b_process_tree_process_exited_(
    struct B_ProcessExitStatus const *status,
    void *opaque,
    B_OUT struct B_Error *e) {
  B_ProcessTree_ *self
    = static_cast<B_ProcessTree_ *>(opaque);
  return self->exited_callback(status, e);
}

struct B_WaitUntilProcessTreeExitedFunctor_ {
  B_WaitUntilProcessTreeExitedFunctor_(
      B_ProcessTree_ *tree) :
      tree(tree) {
  }

  B_ProcessTree_ *tree;

  bool
  operator()() const {
    return this->tree->fully_exited();
  }
};

}

TEST_P(TestProcess, DeeplyNestedProcessTree) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  B_ProcessController *controller;
  ASSERT_TRUE(b_process_manager_get_controller(
    manager, &controller, &e));

  B_ProcessExitStatus success;
  success.type = B_PROCESS_EXIT_STATUS_CODE;
  success.code.exit_code = 0;

  B_ProcessTree_ process_tree(
    controller,
    success,
    b_vec_("true"),
    b_vec_<B_ProcessTree_>());
  for (size_t i = 0; i < 50; ++i) {
    process_tree = B_ProcessTree_(
      controller,
      success,
      b_vec_("true"),
      b_vec_(process_tree));
  }

  ASSERT_TRUE(process_tree.exec(&e));

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilProcessTreeExitedFunctor_(&process_tree),
    &e));
  EXPECT_FALSE(timed_out);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

TEST_P(TestProcess, VeryBranchyProcessTree) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  B_ProcessController *controller;
  ASSERT_TRUE(b_process_manager_get_controller(
    manager, &controller, &e));

  B_ProcessExitStatus success;
  success.type = B_PROCESS_EXIT_STATUS_CODE;
  success.code.exit_code = 0;

  B_ProcessTree_ leaf_process_tree(
    controller,
    success,
    b_vec_("true"),
    b_vec_<B_ProcessTree_>());
  B_ProcessTree_ branch_process_tree(
    controller,
    success,
    b_vec_("true"),
    std::vector<B_ProcessTree_>(10, leaf_process_tree));
  B_ProcessTree_ process_tree(
    controller,
    success,
    b_vec_("true"),
    std::vector<B_ProcessTree_>(10, branch_process_tree));
  ASSERT_TRUE(process_tree.exec(&e));

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilProcessTreeExitedFunctor_(&process_tree),
    &e));
  EXPECT_FALSE(timed_out);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

#if B_CONFIG_POSIX_SIGNALS
TEST_P(TestProcess, BasicExecChildHasCleanSignalMask) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  size_t exit_count = 0;
  B_ProcessExitStatus exit_status;
  b_exec_basic_(
    manager,
    b_vec_(
      b_test_process_child_path_(),
      "BasicExecChildHasCleanSignalMask"),
    &exit_count,
    &exit_status);

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilExitedFunctor_(&exit_count),
    &e));
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(1U, exit_count);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_EQ(0, exit_status.code.exit_code);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}

TEST_P(TestProcess, BasicExecChildDoesNotInheritSigaction) {
  struct B_Error e;
  Tester *tester = this->create_tester();

  // Ignore SIGALRM.  The child process should have
  // SIG_DFL for SIGALRM.
  struct sigaction action;
  action.sa_flags = 0;
  action.sa_handler = SIG_IGN;
  EXPECT_EQ(0, sigemptyset(&action.sa_mask));
  B_SigactionHolder_ holder;
  holder.arm(SIGALRM, action);

  B_ProcessManager *manager;
  ASSERT_TRUE(b_process_manager_allocate(
    tester, &manager, &e));

  size_t exit_count = 0;
  B_ProcessExitStatus exit_status;
  b_exec_basic_(
    manager,
    b_vec_(
      b_test_process_child_path_(),
      "BasicExecChildDoesNotInheritSigaction"),
    &exit_count,
    &exit_status);

  bool timed_out;
  EXPECT_TRUE(tester->wait_and_notify_until(
    manager,
    1 * B_NS_PER_S_,
    &timed_out,
    B_WaitUntilExitedFunctor_(&exit_count),
    &e));
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(1U, exit_count);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_EQ(0, exit_status.code.exit_code);

  EXPECT_TRUE(b_process_manager_deallocate(manager, &e));
  delete tester;
}
#endif
