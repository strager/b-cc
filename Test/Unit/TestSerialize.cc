#include <B/Error.h>
#include <B/Memory.h>
#include <B/Serialize.h>

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
