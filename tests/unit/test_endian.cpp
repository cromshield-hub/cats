#include "libsed/core/endian.h"
#include "libsed/core/types.h"
#include <cassert>
#include <cstdio>

#ifndef TEST
#define TEST(suite, name) void test_##suite##_##name()
#define EXPECT_EQ(a, b) assert((a) == (b))
#define RUN_TEST(suite, name) do { printf("  " #suite "." #name "..."); test_##suite##_##name(); printf(" OK\n"); } while(0)
#endif

using namespace libsed;

TEST(Endian, ReadWrite16) {
    uint8_t buf[2];
    Endian::writeBe16(buf, 0x1234);
    EXPECT_EQ(buf[0], 0x12);
    EXPECT_EQ(buf[1], 0x34);
    EXPECT_EQ(Endian::readBe16(buf), 0x1234);
}

TEST(Endian, ReadWrite32) {
    uint8_t buf[4];
    Endian::writeBe32(buf, 0xDEADBEEF);
    EXPECT_EQ(buf[0], 0xDE);
    EXPECT_EQ(buf[3], 0xEF);
    EXPECT_EQ(Endian::readBe32(buf), 0xDEADBEEFu);
}

TEST(Endian, ReadWrite64) {
    uint8_t buf[8];
    Endian::writeBe64(buf, 0x0102030405060708ULL);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[7], 0x08);
    EXPECT_EQ(Endian::readBe64(buf), 0x0102030405060708ULL);
}

TEST(Endian, MinBytesUnsigned) {
    EXPECT_EQ(Endian::minBytesUnsigned(0), 0u);
    EXPECT_EQ(Endian::minBytesUnsigned(1), 1u);
    EXPECT_EQ(Endian::minBytesUnsigned(255), 1u);
    EXPECT_EQ(Endian::minBytesUnsigned(256), 2u);
    EXPECT_EQ(Endian::minBytesUnsigned(65536), 3u);
}

TEST(Endian, DecodeUnsigned) {
    uint8_t data[] = {0x01, 0x02};
    EXPECT_EQ(Endian::decodeUnsigned(data, 2), 0x0102u);
    EXPECT_EQ(Endian::decodeUnsigned(data, 1), 0x01u);
}

TEST(Endian, Uid) {
    Uid uid(0x0000000900030001ULL);
    EXPECT_EQ(uid.toUint64(), 0x0000000900030001ULL);
    EXPECT_EQ(uid.bytes[0], 0x00);
    EXPECT_EQ(uid.bytes[4], 0x00);
    EXPECT_EQ(uid.bytes[5], 0x03);
    EXPECT_EQ(uid.bytes[7], 0x01);
}

TEST(Endian, UidEquality) {
    Uid a(0x12345678);
    Uid b(0x12345678);
    Uid c(0x87654321);
    EXPECT_EQ(a, b);
    assert(a != c);
}

#ifndef GTEST_INCLUDE_GTEST_GTEST_H_
void run_endian_tests() {
    printf("Endian tests:\n");
    RUN_TEST(Endian, ReadWrite16);
    RUN_TEST(Endian, ReadWrite32);
    RUN_TEST(Endian, ReadWrite64);
    RUN_TEST(Endian, MinBytesUnsigned);
    RUN_TEST(Endian, DecodeUnsigned);
    RUN_TEST(Endian, Uid);
    RUN_TEST(Endian, UidEquality);
    printf("  All Endian tests passed!\n\n");
}
#endif
