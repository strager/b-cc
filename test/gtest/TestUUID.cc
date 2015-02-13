#include <B/UUID.h>

#include <gtest/gtest.h>

TEST(TestUUID, UUIDInitializer) {
    B_UUID const uuid = B_UUID_INITIALIZER(
        12345678, 9ABC, DEF0, 0000, 000000000000);

    // FIXME(strager): Bytes aren't ordered properly.
    EXPECT_EQ(0x12, uuid.data[0]);
    EXPECT_EQ(0x34, uuid.data[1]);
    EXPECT_EQ(0x56, uuid.data[2]);
    EXPECT_EQ(0x78, uuid.data[3]);
    EXPECT_EQ(0x9A, uuid.data[4]);
    EXPECT_EQ(0xBC, uuid.data[5]);
    EXPECT_EQ(0xDE, uuid.data[6]);
    EXPECT_EQ(0xF0, uuid.data[7]);
    EXPECT_EQ(0x00, uuid.data[8]);
}
