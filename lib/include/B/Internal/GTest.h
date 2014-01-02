#ifndef GTEST_H_44107B0C_4BD6_4378_9C5B_2E87D4591B23
#define GTEST_H_44107B0C_4BD6_4378_9C5B_2E87D4591B23

#if !defined(__cplusplus)
#error This is a C++-only header.
#endif

#include <B/Exception.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>

#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <pthread.h>
#include <type_traits>
#include <vector>

// HACK(strager)!
namespace testing {
namespace internal {
class GTEST_API_ UnitTestImpl {
public:
    TestPartResultReporterInterface *
    GetTestPartResultReporterForCurrentThread(
        );

    void
    SetTestPartResultReporterForCurrentThread(
        TestPartResultReporterInterface *);
};
}
}

// Allows assertions to be passed across tests.  Create in
// the main thread, then call "capture" in other threads.
// Call "check_results" periodically in the main thread
// through the B_MT_ASSERT_CHECK() macro.
class B_MultithreadAssert {
public:
    inline
    B_MultithreadAssert() :
        mutex(PTHREAD_MUTEX_INITIALIZER) {
    }

    inline
    ~B_MultithreadAssert() {
        // this->mutex does not need explicit deallocation.

        // TODO(strager): Verify that no threads are
        // capturing.
        assert(this->results.empty());
    }

    // Non-copyable and non-movable, because pthread_mutex_t
    // is neither.
    B_MultithreadAssert(B_MultithreadAssert const &) = delete;
    B_MultithreadAssert(B_MultithreadAssert &&) = delete;
    B_MultithreadAssert &operator =(B_MultithreadAssert const &) = delete;
    B_MultithreadAssert &operator =(B_MultithreadAssert &&) = delete;

    template<typename TFunc>
    typename std::result_of<TFunc()>::type
    capture(
        const TFunc &func) {

        CaptureScope capture_scope(*this);
        return func();
    }

    // Returns 'true' on fatal error.
    inline bool
    check_results() {
        // TODO(strager): Make exception-safe.
        int rc;

        rc = pthread_mutex_lock(&this->mutex);
        assert(rc == 0);

        bool fatal = false;
        for (auto const &result : this->results) {
            if (result.passed()) {
                continue;
            }
            ::testing::internal::AssertHelper(
                result.type(),
                result.file_name(),
                result.line_number(),
                result.message())
                = ::testing::Message();
            if (result.fatally_failed()) {
                fatal = true;
                break;
            }
        }

        this->results.clear();

        rc = pthread_mutex_unlock(&this->mutex);
        assert(rc == 0);

        return fatal;
    }

    inline void
    add_result(::testing::TestPartResult const &result) {
        // TODO(strager): Make exception-safe.
        int rc;

        rc = pthread_mutex_lock(&this->mutex);
        assert(rc == 0);

        this->results.push_back(result);

        rc = pthread_mutex_unlock(&this->mutex);
        assert(rc == 0);
    }

private:
    class CaptureScope :
        public ::testing::TestPartResultReporterInterface {

    public:
        inline
        CaptureScope(B_MultithreadAssert &mt_assert) :
            mt_assert(mt_assert),
            old_reporter(testing::internal::GetUnitTestImpl()
                ->GetTestPartResultReporterForCurrentThread()) {

            testing::internal::GetUnitTestImpl()
                ->SetTestPartResultReporterForCurrentThread(
                    this);
        }

        virtual inline
        ~CaptureScope() {
            testing::internal::GetUnitTestImpl()
                ->SetTestPartResultReporterForCurrentThread(
                    old_reporter);
        }

        virtual inline void
        ReportTestPartResult(
            ::testing::TestPartResult const &result) {

            this->mt_assert.add_result(result);
        }

    private:
        B_MultithreadAssert &mt_assert;
        testing::TestPartResultReporterInterface *old_reporter;
    };

    pthread_mutex_t mutex;
    std::vector<::testing::TestPartResult> results;
};

#define B_MT_ASSERT_CHECK(mt_assert) \
    do { \
        if ((mt_assert).check_results()) { \
            return; \
        } \
    } while (0)

// Checks that an exception did not occur.
#define B_CHECK_EX(ex) \
    ASSERT_EQ(nullptr, (ex))

// Like EXPECT_STREQ, but compares bytes of memory.
#define B_EXPECT_MEMEQ(expected, expected_size, actual, actual_size) \
    do { \
        size_t _bem_expected_size = (expected_size); \
        size_t _bem_actual_size = (actual_size); \
        uint8_t const *_bem_expected \
            = reinterpret_cast<uint8_t const *>((expected)); \
        uint8_t const *_bem_actual \
            = reinterpret_cast<uint8_t const *>((actual)); \
        EXPECT_EQ(_bem_expected_size, _bem_actual_size); \
        size_t _bem_min_size = b_min_size( \
            _bem_expected_size, \
            _bem_actual_size); \
        for (size_t _i = 0; _i < _bem_min_size; ++_i) { \
            EXPECT_EQ(_bem_expected[_i], _bem_actual[_i]) \
                << "Index: " << _i; \
            /* Don't spam the user with failures. */ \
            if (_bem_expected[_i] != _bem_actual[_i]) { \
                break; \
            } \
        } \
    } while (0)

// Receives data from a ZeroMQ socket and compares it with
// expected bytes of memory.
#define B_EXPECT_RECV(expected, expected_size, socket_zmq, flags) \
    do { \
        size_t _ber_expected_size = (expected_size); \
        uint8_t _ber_recv_buffer[_ber_expected_size]; \
        size_t _ber_recv_bytes = _ber_expected_size; \
        B_CHECK_EX(b_zmq_recv( \
            socket_zmq, \
            _ber_recv_buffer, \
            &_ber_recv_bytes, \
            (flags))); \
        EXPECT_EQ(_ber_expected_size, _ber_recv_bytes); \
        B_EXPECT_MEMEQ( \
            _ber_recv_buffer, \
            b_min_size( \
                _ber_recv_bytes, \
                _ber_expected_size), \
            (expected), \
            _ber_expected_size); \
    } while (0)

// Receives a RequestID from a ZeroMQ socket and compares it
// with an expected RequestID.
#define B_EXPECT_RECV_REQUEST_ID_EQ(expected, socket_zmq) \
    do { \
        B_RequestID _received_request_id; \
        B_CHECK_EX(b_protocol_recv_request_id( \
            (socket_zmq), \
            &_received_request_id, \
            0));  /* flags */ \
        B_EXPECT_REQUEST_ID_EQ( \
            expected, \
            _received_request_id); \
    } while (0)

#define B_EXPECT_REQUEST_ID_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).bytes[0], (actual).bytes[0]); \
        EXPECT_EQ((expected).bytes[1], (actual).bytes[1]); \
        EXPECT_EQ((expected).bytes[2], (actual).bytes[2]); \
        EXPECT_EQ((expected).bytes[3], (actual).bytes[3]); \
    } while (0)

// TODO(strager): Factor duplication.
#define B_EXPECT_REQUEST_ID_NE(expected, actual) \
    do { \
        EXPECT_NE((expected).bytes[0], (actual).bytes[0]); \
        EXPECT_NE((expected).bytes[1], (actual).bytes[1]); \
        EXPECT_NE((expected).bytes[2], (actual).bytes[2]); \
        EXPECT_NE((expected).bytes[3], (actual).bytes[3]); \
    } while (0)

#endif
