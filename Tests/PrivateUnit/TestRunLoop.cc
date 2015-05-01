#include <B/Error.h>
#include <B/Private/Config.h>
#include <B/RunLoop.h>

#include <gtest/gtest.h>
#include <vector>

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

class PlainRunLoopFactory : public RunLoopFactory {
public:
  virtual struct B_RunLoop *
  create() const override {
    return RunLoopFactory::create(
      b_run_loop_allocate_plain);
  }

  virtual const char *
  get_type_string() const override {
    return "Plain";
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
  new PlainRunLoopFactory(),
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
