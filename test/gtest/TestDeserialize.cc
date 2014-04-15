#include "Mocks.h"

#include <B/Deserialize.h>

#include <gtest/gtest.h>

template<int N>
B_FUNC
b_byte_source_in_memory_initialize(
        B_OUT B_ByteSourceInMemory *storage,
        uint8_t const data[N],
        B_BORROWED B_OUTPTR B_ByteSource **source,
        B_ErrorHandler const *eh) {
    return b_byte_source_in_memory_initialize(
        storage,
        data,
        N,
        source,
        eh);
}

TEST(TestDeserialize, EmptyInMemoryEOF) {
    B_ErrorHandler const *eh = nullptr;

    uint8_t data;
    B_ByteSourceInMemory storage;
    B_ByteSource *source;
    ASSERT_TRUE(b_byte_source_in_memory_initialize(
        &storage,
        &data,
        0,
        &source,
        eh));

    {
        MockErrorHandler mock_eh;
        uint8_t x;
        EXPECT_CALL(mock_eh, handle_error(B_Error{ENOSPC}))
            .WillOnce(Return(B_ERROR_ABORT));
        ASSERT_FALSE(
            b_deserialize_1(source, &x, &mock_eh));
    }

    {
        MockErrorHandler mock_eh;
        uint16_t x;
        EXPECT_CALL(mock_eh, handle_error(B_Error{ENOSPC}))
            .WillOnce(Return(B_ERROR_ABORT));
        ASSERT_FALSE(
            b_deserialize_2_be(source, &x, &mock_eh));
    }

    {
        MockErrorHandler mock_eh;
        uint32_t x;
        EXPECT_CALL(mock_eh, handle_error(B_Error{ENOSPC}))
            .WillOnce(Return(B_ERROR_ABORT));
        ASSERT_FALSE(
            b_deserialize_4_be(source, &x, &mock_eh));
    }

    {
        MockErrorHandler mock_eh;
        uint64_t x;
        EXPECT_CALL(mock_eh, handle_error(B_Error{ENOSPC}))
            .WillOnce(Return(B_ERROR_ABORT));
        ASSERT_FALSE(
            b_deserialize_8_be(source, &x, &mock_eh));
    }
}

TEST(TestDeserialize, BigEndianWordsInMemory) {
    B_ErrorHandler const *eh = nullptr;

    uint8_t data[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    };
    B_ByteSourceInMemory storage;
    B_ByteSource *source;
    ASSERT_TRUE(b_byte_source_in_memory_initialize(
        &storage,
        data,
        sizeof(data),
        &source,
        eh));

    uint8_t x1;
    ASSERT_TRUE(b_deserialize_1(source, &x1, eh));
    EXPECT_EQ(0x01, x1);

    uint16_t x2;
    ASSERT_TRUE(b_deserialize_2_be(source, &x2, eh));
    EXPECT_EQ(0x0203, x2);

    uint32_t x4;
    ASSERT_TRUE(b_deserialize_4_be(source, &x4, eh));
    EXPECT_EQ(static_cast<uint32_t>(0x04050607), x4);

    uint64_t x8;
    ASSERT_TRUE(b_deserialize_8_be(source, &x8, eh));
    EXPECT_EQ(
        static_cast<uint64_t>(0x08090A0B0C0D0E0F),
        x8);
}
