#include <B/Error.h>
#include <B/Memory.h>
#include <B/Serialize.h>

#include <errno.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

TEST(TestSerialize, EmptyInMemory) {
  struct B_Error e;

  struct B_ByteSinkInMemory storage;
  struct B_ByteSink *sink;
  ASSERT_TRUE(b_byte_sink_in_memory_initialize(
    &storage, &sink, &e));

  uint8_t *data;
  size_t data_size;
  ASSERT_TRUE(b_byte_sink_in_memory_finalize(
    &storage, &data, &data_size, &e));
  ASSERT_TRUE(data);
  ASSERT_EQ(static_cast<size_t>(0), data_size);

  b_deallocate(data);
}

TEST(TestSerialize, EmptyInMemoryRelease) {
  struct B_Error e;

  struct B_ByteSinkInMemory storage;
  struct B_ByteSink *sink;
  ASSERT_TRUE(b_byte_sink_in_memory_initialize(
    &storage, &sink, &e));
  ASSERT_TRUE(b_byte_sink_in_memory_release(
    &storage, &e));
  // Leak checker should ensure data is released.
}

TEST(TestSerialize, BigEndianWordsInMemory) {
  struct B_Error e;

  struct B_ByteSinkInMemory storage;
  struct B_ByteSink *sink;
  ASSERT_TRUE(b_byte_sink_in_memory_initialize(
    &storage, &sink, &e));

  ASSERT_TRUE(b_serialize_1(sink, 0x01, &e));
  ASSERT_TRUE(b_serialize_2_be(sink, 0x0203, &e));
  ASSERT_TRUE(b_serialize_4_be(sink, 0x04050607, &e));
  ASSERT_TRUE(
      b_serialize_8_be(sink, 0x08090A0B0C0D0E0F, &e));

  uint8_t *data;
  size_t data_size;
  ASSERT_TRUE(b_byte_sink_in_memory_finalize(
    &storage, &data, &data_size, &e));
  ASSERT_TRUE(data);
  ASSERT_EQ(static_cast<size_t>(15), data_size);
  for (size_t i = 0; i < 15; ++i) {
    EXPECT_EQ(static_cast<uint8_t>(i + 1), data[i])
      << "i = " << i;
  }

  b_deallocate(data);
}

TEST(TestDeserialize, EmptyInMemoryEOF) {
  struct B_Error e;

  uint8_t data;
  struct B_ByteSourceInMemory storage;
  struct B_ByteSource *source;
  ASSERT_TRUE(b_byte_source_in_memory_initialize(
    &storage, &data, 0, &source, &e));

  uint8_t x1;
  EXPECT_FALSE(b_deserialize_1(source, &x1, &e));
  EXPECT_EQ(ENOSPC, e.posix_error);

  uint16_t x2;
  EXPECT_FALSE(b_deserialize_2_be(source, &x2, &e));
  EXPECT_EQ(ENOSPC, e.posix_error);

  uint32_t x4;
  EXPECT_FALSE(b_deserialize_4_be(source, &x4, &e));
  EXPECT_EQ(ENOSPC, e.posix_error);

  uint64_t x8;
  EXPECT_FALSE(b_deserialize_8_be(source, &x8, &e));
  EXPECT_EQ(ENOSPC, e.posix_error);
}

TEST(TestDeserialize, BigEndianWordsInMemory) {
  struct B_Error e;

  uint8_t data[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  };
  struct B_ByteSourceInMemory storage;
  struct B_ByteSource *source;
  ASSERT_TRUE(b_byte_source_in_memory_initialize(
    &storage, data, sizeof(data), &source, &e));

  uint8_t x1;
  ASSERT_TRUE(b_deserialize_1(source, &x1, &e));
  EXPECT_EQ(0x01, x1);

  uint16_t x2;
  ASSERT_TRUE(b_deserialize_2_be(source, &x2, &e));
  EXPECT_EQ(0x0203, x2);

  uint32_t x4;
  ASSERT_TRUE(b_deserialize_4_be(source, &x4, &e));
  EXPECT_EQ(static_cast<uint32_t>(0x04050607), x4);

  uint64_t x8;
  ASSERT_TRUE(b_deserialize_8_be(source, &x8, &e));
  EXPECT_EQ(static_cast<uint64_t>(0x08090A0B0C0D0E0F), x8);
}
