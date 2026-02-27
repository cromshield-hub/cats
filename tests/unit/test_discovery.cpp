#include "libsed/discovery/discovery.h"
#include "libsed/core/endian.h"
#include <cassert>
#include <cstdio>

#ifndef TEST
#define TEST(suite, name) void test_##suite##_##name()
#define EXPECT_EQ(a, b) assert((a) == (b))
#define EXPECT_TRUE(a) assert(a)
#define EXPECT_FALSE(a) assert(!(a))
#define RUN_TEST(suite, name) do { printf("  " #suite "." #name "..."); test_##suite##_##name(); printf(" OK\n"); } while(0)
#endif

using namespace libsed;

static Bytes buildDiscoveryResponse(uint16_t sscFeatureCode, uint16_t baseComId) {
    Bytes resp(256, 0);
    size_t offset = 0;

    // Header
    Endian::writeBe32(resp.data(), 108 - 4); // total length
    Endian::writeBe16(resp.data() + 4, 0);   // major
    Endian::writeBe16(resp.data() + 6, 1);   // minor
    offset = 48;

    // TPer feature
    Endian::writeBe16(resp.data() + offset, 0x0001);
    resp[offset + 2] = 0x10;
    resp[offset + 3] = 16;
    resp[offset + 4] = 0x01;
    offset += 20;

    // Locking feature
    Endian::writeBe16(resp.data() + offset, 0x0002);
    resp[offset + 2] = 0x10;
    resp[offset + 3] = 16;
    resp[offset + 4] = 0x07;
    offset += 20;

    // SSC feature
    Endian::writeBe16(resp.data() + offset, sscFeatureCode);
    resp[offset + 2] = 0x10;
    resp[offset + 3] = 16;
    Endian::writeBe16(resp.data() + offset + 4, baseComId);
    Endian::writeBe16(resp.data() + offset + 6, 1);

    return resp;
}

TEST(Discovery, ParseOpalV2) {
    auto data = buildDiscoveryResponse(0x0203, 0x0001);
    Discovery disc;
    auto r = disc.parse(data);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(disc.detectSsc(), SscType::Opal20);
    EXPECT_EQ(disc.baseComId(), 0x0001);
    EXPECT_TRUE(disc.hasTPerFeature());
    EXPECT_TRUE(disc.hasLockingFeature());
    EXPECT_TRUE(disc.hasOpalV2Feature());
}

TEST(Discovery, ParseEnterprise) {
    auto data = buildDiscoveryResponse(0x0100, 0x0002);
    Discovery disc;
    auto r = disc.parse(data);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(disc.detectSsc(), SscType::Enterprise);
    EXPECT_EQ(disc.baseComId(), 0x0002);
    EXPECT_TRUE(disc.hasEnterpriseFeature());
    EXPECT_FALSE(disc.hasOpalV2Feature());
}

TEST(Discovery, ParsePyrite) {
    auto data = buildDiscoveryResponse(0x0302, 0x0003);
    Discovery disc;
    auto r = disc.parse(data);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(disc.detectSsc(), SscType::Pyrite10);
    EXPECT_TRUE(disc.hasPyriteV1Feature());
}

TEST(Discovery, BuildInfo) {
    auto data = buildDiscoveryResponse(0x0203, 0x0001);
    Discovery disc;
    disc.parse(data);

    auto info = disc.buildInfo();
    EXPECT_EQ(info.primarySsc, SscType::Opal20);
    EXPECT_EQ(info.baseComId, 0x0001);
    EXPECT_TRUE(info.tperPresent);
    EXPECT_TRUE(info.lockingPresent);
    EXPECT_TRUE(info.lockingEnabled);
    EXPECT_TRUE(info.locked);
}

TEST(Discovery, EmptyResponse) {
    Bytes empty(10, 0);
    Discovery disc;
    auto r = disc.parse(empty);
    EXPECT_TRUE(r.failed());
}

#ifndef GTEST_INCLUDE_GTEST_GTEST_H_
void run_discovery_tests() {
    printf("Discovery tests:\n");
    RUN_TEST(Discovery, ParseOpalV2);
    RUN_TEST(Discovery, ParseEnterprise);
    RUN_TEST(Discovery, ParsePyrite);
    RUN_TEST(Discovery, BuildInfo);
    RUN_TEST(Discovery, EmptyResponse);
    printf("  All Discovery tests passed!\n\n");
}
#endif
