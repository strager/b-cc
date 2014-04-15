#include <B/Alloc.h>
#include <B/Serialize.h>

#include <gtest/gtest.h>

TEST(TestSerialize, EmptyInMemory) {
    B_ErrorHandler const *eh = nullptr;

    B_ByteSinkInMemory storage;
    B_ByteSink *sink;
    ASSERT_TRUE(b_byte_sink_in_memory_initialize(
        &storage,
        &sink,
        eh));

    uint8_t *data;
    size_t data_size;
    ASSERT_TRUE(b_byte_sink_in_memory_finalize(
        &storage,
        &data,
        &data_size,
        eh));

    ASSERT_NE(nullptr, data);
    ASSERT_EQ(static_cast<size_t>(0), data_size);

    ASSERT_TRUE(b_deallocate(data, eh));
}

TEST(TestSerialize, EmptyInMemoryRelease) {
    B_ErrorHandler const *eh = nullptr;

    B_ByteSinkInMemory storage;
    B_ByteSink *sink;
    ASSERT_TRUE(b_byte_sink_in_memory_initialize(
        &storage,
        &sink,
        eh));
    ASSERT_TRUE(b_byte_sink_in_memory_release(
        &storage,
        eh));
    // Leak checker should ensure data is released.
}

TEST(TestSerialize, BigEndianWordsInMemory) {
    B_ErrorHandler const *eh = nullptr;

    B_ByteSinkInMemory storage;
    B_ByteSink *sink;
    ASSERT_TRUE(b_byte_sink_in_memory_initialize(
        &storage,
        &sink,
        eh));

    ASSERT_TRUE(b_serialize_1(sink, 0x01, eh));
    ASSERT_TRUE(b_serialize_2_be(sink, 0x0203, eh));
    ASSERT_TRUE(b_serialize_4_be(sink, 0x04050607, eh));
    ASSERT_TRUE(
        b_serialize_8_be(sink, 0x08090A0B0C0D0E0F, eh)); 

    uint8_t *data;
    size_t data_size;
    ASSERT_TRUE(b_byte_sink_in_memory_finalize(
        &storage,
        &data,
        &data_size,
        eh));

    ASSERT_NE(nullptr, data);
    ASSERT_EQ(static_cast<size_t>(15), data_size);

    for (size_t i = 0; i < 15; ++i) {
        EXPECT_EQ(static_cast<uint8_t>(i + 1), data[i])
            << "i = " << i;
    }

    ASSERT_TRUE(b_deallocate(data, eh));
}
