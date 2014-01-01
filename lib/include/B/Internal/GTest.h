#ifndef GTEST_H_44107B0C_4BD6_4378_9C5B_2E87D4591B23
#define GTEST_H_44107B0C_4BD6_4378_9C5B_2E87D4591B23

#if !defined(__cplusplus)
#error This is a C++-only header.
#endif

#include <B/Exception.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>

#include <gtest/gtest.h>

#include <cstdint>

// Checks that an exception did not occur.
#define B_CHECK_EX(ex) \
    ASSERT_EQ(nullptr, (ex))

// Like EXPECT_STREQ, but compares bytes of memory.
#define B_EXPECT_MEMEQ(expected, expected_size, actual, actual_size) \
    do { \
        size_t _bem_expected_size = (expected_size); \
        size_t _bem_actual_size = (actual_size); \
        uint8_t const *_bem_expected = (expected); \
        uint8_t const *_bem_actual = (actual); \
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

#endif
