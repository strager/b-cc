#include <B/Error.h>
#include <B/Private/Config.h>
#include <B/Process.h>
#include <B/RunLoop.h>

#if B_CONFIG_POSIX_SPAWN
# include "Environ.h"
#endif

#include <errno.h>
#include <gtest/gtest.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#if B_CONFIG_POSIX_SPAWN
# include <spawn.h>
#endif

#if __cplusplus < 201100L
# define override
#endif

namespace {

struct B_RunLoopClosure_ {
  // Each true is a callback call. Each false is a cancel
  // callback call.
  std::vector<bool> calls;
};

B_FUNC bool
b_run_loop_function_stop_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *) {
  B_RunLoopClosure_ *closure
    = *static_cast<B_RunLoopClosure_ *const *>(opaque);
  closure->calls.push_back(true);
  struct B_Error e;
  EXPECT_TRUE(b_run_loop_stop(rl, &e));
  return true;
}

B_FUNC bool
b_run_loop_function_fail_(
    B_BORROW struct B_RunLoop *,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *) {
  B_RunLoopClosure_ *closure
    = *static_cast<B_RunLoopClosure_ *const *>(opaque);
  closure->calls.push_back(false);
  ADD_FAILURE();
  return true;
}

B_FUNC bool
b_run_loop_function_noop_(
    B_BORROW struct B_RunLoop *,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *) {
  B_RunLoopClosure_ *closure
    = *static_cast<B_RunLoopClosure_ *const *>(opaque);
  closure->calls.push_back(false);
  return true;
}

B_FUNC bool
b_run_loop_function_stop_no_closure_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *) {
  struct B_Error e;
  EXPECT_TRUE(b_run_loop_stop(rl, &e));
  return true;
}

B_FUNC bool
b_run_loop_function_fail_no_closure_(
    B_BORROW struct B_RunLoop *,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *) {
  ADD_FAILURE();
  return true;
}

}

namespace {

class RunLoopFactory {
public:
  virtual struct B_RunLoop *
  create() const = 0;

  virtual const char *
  get_type_string() const = 0;

protected:
  static struct B_RunLoop *
  create(
      B_FUNC bool (*f)(
        B_OUT_TRANSFER struct B_RunLoop **,
        B_OUT struct B_Error *)) {
    struct B_Error e;
    struct B_RunLoop *rl;
    if (!f(&rl, &e)) {
      ADD_FAILURE();
      return NULL;
    }
    return rl;
  }
};

class KqueueRunLoopFactory : public RunLoopFactory {
public:
  virtual struct B_RunLoop *
  create() const override {
    return RunLoopFactory::create(
      b_run_loop_allocate_kqueue);
  }

  virtual const char *
  get_type_string() const override {
    return "Kqueue";
  }
};

class SigchldRunLoopFactory : public RunLoopFactory {
public:
  virtual struct B_RunLoop *
  create() const override {
    return RunLoopFactory::create(
      b_run_loop_allocate_sigchld);
  }

  virtual const char *
  get_type_string() const override {
    return "SIGCHLD";
  }
};

std::ostream &operator<<(
    std::ostream &stream,
    RunLoopFactory *run_loop_factory) {
  if (run_loop_factory) {
    stream << run_loop_factory->get_type_string();
  } else {
    stream << "(null)";
  }
  return stream;
}

RunLoopFactory *
factories[] = {
#if B_CONFIG_POSIX_SIGNALS
  new SigchldRunLoopFactory(),
#endif
#if B_CONFIG_KQUEUE
  new KqueueRunLoopFactory(),
#endif
};

class TestRunLoop :
  public ::testing::TestWithParam<RunLoopFactory *> {
public:
  struct B_RunLoop *
  create() const {
    return this->GetParam()->create();
  }
};

}

INSTANTIATE_TEST_CASE_P(
  ,  // No prefix.
  TestRunLoop,
  ::testing::ValuesIn(factories));

TEST_P(TestRunLoop, StopFunctionNoClosure) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  ASSERT_TRUE(b_run_loop_add_function(
    rl,
    b_run_loop_function_stop_no_closure_,
    b_run_loop_function_fail_no_closure_,
    NULL,
    0,
    &e));
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
}

TEST_P(TestRunLoop, StopFunctionWithClosure) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  B_RunLoopClosure_ closure;
  B_RunLoopClosure_ *closure_pointer = &closure;
  ASSERT_TRUE(b_run_loop_add_function(
    rl,
    b_run_loop_function_stop_,
    b_run_loop_function_fail_,
    &closure_pointer,
    sizeof(closure_pointer),
    &e));
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  EXPECT_EQ(1U, closure.calls.size());
  if (!closure.calls.empty()) {
    EXPECT_TRUE(closure.calls[0]);
  }
  b_run_loop_deallocate(rl);
}

TEST_P(TestRunLoop, TwoStopFunctions) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  B_RunLoopClosure_ closure;
  B_RunLoopClosure_ *closure_pointer = &closure;
  ASSERT_TRUE(b_run_loop_add_function(
    rl,
    b_run_loop_function_stop_,
    b_run_loop_function_noop_,
    &closure_pointer,
    sizeof(closure_pointer),
    &e));
  ASSERT_TRUE(b_run_loop_add_function(
    rl,
    b_run_loop_function_stop_,
    b_run_loop_function_noop_,
    &closure_pointer,
    sizeof(closure_pointer),
    &e));
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(2U, closure.calls.size());
  if (closure.calls.size() >= 2) {
    // FIXME(strager): This makes assumptions about the
    // scheduling algorithm.
    EXPECT_TRUE(closure.calls[0]);
    EXPECT_FALSE(closure.calls[1]);
  }
}

namespace {

template<typename T>
std::vector<T>
b_vec_() {
  return std::vector<T>();
}

template<typename T>
std::vector<T>
b_vec_(
    T a) {
  std::vector<T> v;
  v.push_back(a);
  return v;
}

template<typename T>
std::vector<T>
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

template<typename TFunc>
struct B_ExecAndStopIfClosure_ {
  B_ExecAndStopIfClosure_(
      struct B_ProcessExitStatus *exit_status,
      TFunc stop_check) :
      exit_status(exit_status),
      stop_check(stop_check) {
  }

  struct B_ProcessExitStatus *exit_status;
  // FIXME(strager): Is there a way to enforce that TFunc is
  // memcpy-able?
  TFunc stop_check;
};

template<typename TFunc>
B_FUNC bool
b_exec_and_stop_if_callback_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW struct B_ProcessExitStatus const *exit_status,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *) {
  B_ExecAndStopIfClosure_<TFunc> const *closure
    = static_cast<B_ExecAndStopIfClosure_<TFunc> const *>(
      opaque);
  *closure->exit_status = *exit_status;
  if (closure->stop_check()) {
    struct B_Error e;
    EXPECT_TRUE(b_run_loop_stop(rl, &e));
  }
  return true;
}

bool
b_exec_pid_(
    B_BORROW std::vector<char const *> args,
    B_OUT pid_t *out,
    struct B_Error *e) {
#if B_CONFIG_POSIX_SPAWN
  args.push_back(NULL);
  pid_t pid;
  int rc = posix_spawnp(
    &pid,
    args.front(),
    NULL,
    NULL,
    const_cast<char *const *>(args.data()),
    get_environ());
  if (rc != 0) {
    e->posix_error = rc;
    return false;
  }
  *out = pid;
  return true;
#else
# error "Unknown process start implementation"
#endif
}

bool
b_exec_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW std::vector<char const *> args,
    B_RunLoopProcessFunction *callback,
    B_RunLoopFunction *cancel,
    B_BORROW void const *callback_data,
    size_t callback_data_size,
    struct B_Error *e) {
#if B_CONFIG_POSIX_SPAWN
  pid_t pid;
  if (!b_exec_pid_(args, &pid, e)) {
    return false;
  }
  if (!b_run_loop_add_process_id(
      rl,
      pid,
      callback,
      cancel,
      callback_data,
      callback_data_size,
      e)) {
    return false;
  }
  return true;
#else
# error "Unknown process start implementation"
#endif
}

// TFunc must be memcpy-able.
template<typename TFunc>
void
b_exec_and_stop_if_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW std::vector<char const *> args,
    // Asynchronously set.
    B_OUT struct B_ProcessExitStatus *exit_status,
    TFunc stop_check) {
  B_ExecAndStopIfClosure_<TFunc> closure(
    exit_status, stop_check);
  struct B_Error e;
  if (!b_exec_(
      rl,
      args,
      b_exec_and_stop_if_callback_<TFunc>,
      b_run_loop_function_fail_,
      &closure,
      sizeof(closure),
      &e)) {
    ADD_FAILURE() << strerror(e.posix_error);
    ASSERT_TRUE(b_run_loop_stop(rl, &e));
  }
}

struct B_True_ {
  bool
  operator()() const {
    return true;
  }
};

void
b_exec_and_stop_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW std::vector<char const *> args,
    // Asynchronously set.
    B_OUT struct B_ProcessExitStatus *exit_status) {
  b_exec_and_stop_if_(rl, args, exit_status, B_True_());
}

}

TEST_P(TestRunLoop, TrueProcess) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  B_ProcessExitStatus exit_status;
  b_exec_and_stop_(rl, b_vec_("true"), &exit_status);
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_EQ(0, exit_status.u.code.exit_code);
}

TEST_P(TestRunLoop, FalseProcess) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  B_ProcessExitStatus exit_status;
  b_exec_and_stop_(rl, b_vec_("false"), &exit_status);
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_EQ(1, exit_status.u.code.exit_code);
}

#if B_CONFIG_POSIX_SIGNALS
TEST_P(TestRunLoop, KillSIGTERM) {
#else
TEST_P(TestRunLoop, DISABLED_KillSIGTERM) {
#endif
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  B_ProcessExitStatus exit_status;
  b_exec_and_stop_(
    rl, b_vec_("sh", "-c", "kill -TERM $$"), &exit_status);
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_SIGNAL, exit_status.type);
  EXPECT_EQ(SIGTERM, exit_status.u.signal.signal_number);
}

struct B_IncrementTrueIfEqual_ {
  B_IncrementTrueIfEqual_(
      size_t *counter,
      size_t stop_at) :
      counter(counter),
      stop_at(stop_at) {
  }

  bool
  operator()() const {
    ++*this->counter;
    return *this->counter >= this->stop_at;
  }

  size_t *counter;
  size_t stop_at;
};

TEST_P(TestRunLoop, MultipleTruesInParallel) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
  size_t const process_count = 20;
  size_t exit_count = 0;
  std::vector<B_ProcessExitStatus> exit_statuses(
    process_count);
  for (size_t i = 0; i < process_count; ++i) {
    b_exec_and_stop_if_(
      rl,
      b_vec_("true"),
      &exit_statuses[i],
      B_IncrementTrueIfEqual_(&exit_count, process_count));
  }
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(process_count, exit_count);
  for (size_t i = 0; i < process_count; ++i) {
    EXPECT_EQ(
      B_PROCESS_EXIT_STATUS_CODE, exit_statuses[i].type);
    EXPECT_EQ(0, exit_statuses[i].u.code.exit_code);
  }
}

namespace {

static B_RunLoopProcessFunction
b_process_tree_callback_;

struct B_ProcessTree_ {
private:
  struct B_RunLoop *run_loop;

  struct B_ProcessExitStatus expected_exit_status;

  // Child program arguments.
  std::vector<char const *> program_args;

  // Run these processes when the current process
  // completes.
  std::vector<B_ProcessTree_> next_processes;

  B_ProcessExitStatus exit_status;
  bool exited;

  B_ProcessTree_ *root;

public:
  B_ProcessTree_(
      struct B_RunLoop *run_loop,
      B_ProcessExitStatus expected_exit_status,
      std::vector<char const *> program_args,
      std::vector<B_ProcessTree_> next_processes) :
      run_loop(run_loop),
      expected_exit_status(expected_exit_status),
      program_args(program_args),
      next_processes(next_processes),
      exited(false) {
    this->set_root(this);
  }

  B_ProcessTree_(
      B_ProcessTree_ const &other) :
      run_loop(other.run_loop),
      expected_exit_status(other.expected_exit_status),
      program_args(other.program_args),
      next_processes(other.next_processes),
      exited(other.exited) {
    this->set_root(this);
  }

  B_ProcessTree_ &
  operator=(
      B_ProcessTree_ const &other) {
    // FIXME(strager): We should delegate to the copy
    // constructor.
    this->run_loop = other.run_loop;
    this->expected_exit_status = other.expected_exit_status;
    this->program_args = other.program_args;
    this->next_processes = other.next_processes;
    this->exited = other.exited;
    this->set_root(this);
    return *this;
  }

  B_WUR B_FUNC bool
  exec(
      B_OUT struct B_Error *e) {
    B_ProcessTree_ *self = this;
    return b_exec_(
      this->run_loop,
      this->program_args,
      b_process_tree_callback_,
      b_run_loop_function_fail_,
      &self,
      sizeof(self),
      e);
  }

  bool
  fully_exited() const {
    if (!this->exited) {
      return false;
    }
    for (
        size_t i = 0;
        i < this->next_processes.size();
        ++i) {
      if (!this->next_processes[i].fully_exited()) {
        return false;
      }
    }
    return true;
  }

private:
  void
  set_root(
      B_ProcessTree_ *root) {
    for (
        size_t i = 0;
        i < this->next_processes.size();
        ++i) {
      this->next_processes[i].set_root(root);
    }
    this->root = root;
  }

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
        size_t i = 0;
        i < this->next_processes.size();
        ++i) {
      EXPECT_TRUE(this->next_processes[i].exec(e));
    }
    if (this->root->fully_exited()) {
      EXPECT_TRUE(b_run_loop_stop(this->run_loop, e));
    }
    return true;
  }

  friend B_RunLoopProcessFunction
  b_process_tree_callback_;
};

static B_FUNC bool
b_process_tree_callback_(
    B_BORROW struct B_RunLoop *rl,
    B_BORROW struct B_ProcessExitStatus const *status,
    B_BORROW void const *opaque,
    B_OUT struct B_Error *e) {
  B_ProcessTree_ *self
    = *static_cast<B_ProcessTree_ *const *>(opaque);
  return self->exited_callback(status, e);
}

}

TEST_P(TestRunLoop, DeeplyNestedProcessTree) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();

  B_ProcessExitStatus success;
  success.type = B_PROCESS_EXIT_STATUS_CODE;
  success.u.code.exit_code = 0;

  B_ProcessTree_ process_tree(
    rl, success, b_vec_("true"), b_vec_<B_ProcessTree_>());
  for (size_t i = 0; i < 50; ++i) {
    process_tree = B_ProcessTree_(
      rl, success, b_vec_("true"), b_vec_(process_tree));
  }
  ASSERT_TRUE(process_tree.exec(&e));

  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_TRUE(process_tree.fully_exited());
}

TEST_P(TestRunLoop, VeryBranchyProcessTree) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();

  B_ProcessExitStatus success;
  success.type = B_PROCESS_EXIT_STATUS_CODE;
  success.u.code.exit_code = 0;

  B_ProcessTree_ leaf_process_tree(
    rl, success, b_vec_("true"), b_vec_<B_ProcessTree_>());
  B_ProcessTree_ branch_process_tree(
    rl,
    success,
    b_vec_("true"),
    std::vector<B_ProcessTree_>(10, leaf_process_tree));
  B_ProcessTree_ process_tree(
    rl,
    success,
    b_vec_("true"),
    std::vector<B_ProcessTree_>(10, branch_process_tree));
  ASSERT_TRUE(process_tree.exec(&e));

  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_TRUE(process_tree.fully_exited());
}

static void
b_sleep_ignoring_signals_(
    unsigned seconds) {
  struct timespec ts;
  ts.tv_sec = seconds;
  ts.tv_nsec = 0;
retry:
  int rc = nanosleep(&ts, &ts);
  if (rc == -1) {
    if (errno == EINTR) {
      goto retry;
    } else {
      ADD_FAILURE() << strerror(errno);
    }
  }
}

TEST_P(TestRunLoop, AddExitedProcess) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
#if B_CONFIG_POSIX_SPAWN
  pid_t pid;
  ASSERT_TRUE(b_exec_pid_(b_vec_("true"), &pid, &e));
  b_sleep_ignoring_signals_(5);
  struct B_ProcessExitStatus exit_status;
  B_ExecAndStopIfClosure_<B_True_> closure(
    &exit_status, B_True_());
  ASSERT_TRUE(b_run_loop_add_process_id(
    rl,
    pid,
    b_exec_and_stop_if_callback_<B_True_>,
    b_run_loop_function_fail_,
    &closure,
    sizeof(closure),
    &e));
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_CODE, exit_status.type);
  EXPECT_EQ(0, exit_status.u.code.exit_code);
#else
# error "Unknown process start implementation"
#endif
}

TEST_P(TestRunLoop, AddExitedProcess2) {
  struct B_Error e;
  struct B_RunLoop *rl = this->create();
#if B_CONFIG_POSIX_SPAWN
  pid_t pid;
  ASSERT_TRUE(b_exec_pid_(
    b_vec_("sh", "-c", "kill -TERM $$"), &pid, &e));
  b_sleep_ignoring_signals_(5);
  struct B_ProcessExitStatus exit_status;
  B_ExecAndStopIfClosure_<B_True_> closure(
    &exit_status, B_True_());
  ASSERT_TRUE(b_run_loop_add_process_id(
    rl,
    pid,
    b_exec_and_stop_if_callback_<B_True_>,
    b_run_loop_function_fail_,
    &closure,
    sizeof(closure),
    &e));
  ASSERT_TRUE(b_run_loop_run(rl, &e));
  b_run_loop_deallocate(rl);
  EXPECT_EQ(B_PROCESS_EXIT_STATUS_SIGNAL, exit_status.type);
  EXPECT_EQ(SIGTERM, exit_status.u.signal.signal_number);
#else
# error "Unknown process start implementation"
#endif
}
