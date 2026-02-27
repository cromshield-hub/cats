#include "libsed/packet/packet_builder.h"
#include "libsed/codec/token_encoder.h"
#include <cassert>
#include <cstdio>

#ifndef TEST
#define TEST(suite, name) void test_##suite##_##name()
#define EXPECT_EQ(a, b) assert((a) == (b))
#define EXPECT_TRUE(a) assert(a)
#define RUN_TEST(suite, name) do { printf("  " #suite "." #name "..."); test_##suite##_##name(); printf(" OK\n"); } while(0)
#endif

using namespace libsed;

TEST(Packet, BuildAndParse) {
    PacketBuilder builder;
    builder.setComId(0x0001);
    builder.setSessionNumbers(100, 200);

    TokenEncoder enc;
    enc.encodeUint(42);
    Bytes payload = enc.data();

    auto comPacket = builder.buildComPacket(payload);
    EXPECT_TRUE(comPacket.size() >= 512); // Padded to 512

    PacketBuilder::ParsedResponse resp;
    auto r = builder.parseResponse(comPacket.data(), comPacket.size(), resp);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(resp.comPacketHeader.comId, 0x0001);
    EXPECT_EQ(resp.packetHeader.tperSessionNumber, 100u);
    EXPECT_EQ(resp.packetHeader.hostSessionNumber, 200u);
    EXPECT_EQ(resp.tokenPayload.size(), payload.size());
}

TEST(Packet, SessionManagerPacket) {
    PacketBuilder builder;
    builder.setComId(0x0001);
    builder.setSessionNumbers(999, 888);

    TokenEncoder enc;
    enc.encodeUint(1);
    auto smPacket = builder.buildSessionManagerPacket(enc.data());

    PacketBuilder::ParsedResponse resp;
    auto r = builder.parseResponse(smPacket.data(), smPacket.size(), resp);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(resp.packetHeader.tperSessionNumber, 0u);
    EXPECT_EQ(resp.packetHeader.hostSessionNumber, 0u);
}

TEST(Packet, ComPacketHeader) {
    ComPacketHeader hdr;
    hdr.comId = 0x1234;
    hdr.comIdExtension = 0x5678;
    hdr.length = 100;

    Bytes buf;
    hdr.serialize(buf);
    EXPECT_EQ(buf.size(), ComPacketHeader::HEADER_SIZE);

    ComPacketHeader parsed;
    auto r = ComPacketHeader::deserialize(buf.data(), buf.size(), parsed);
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(parsed.comId, 0x1234);
    EXPECT_EQ(parsed.comIdExtension, 0x5678);
    EXPECT_EQ(parsed.length, 100u);
}

TEST(Packet, Padding) {
    Bytes data = {1, 2, 3};
    PacketBuilder::padTo4(data);
    EXPECT_EQ(data.size(), 4u);
    EXPECT_EQ(data[3], 0);

    Bytes data2 = {1, 2, 3, 4};
    PacketBuilder::padTo4(data2);
    EXPECT_EQ(data2.size(), 4u);
}

#ifndef GTEST_INCLUDE_GTEST_GTEST_H_
void run_packet_tests() {
    printf("Packet tests:\n");
    RUN_TEST(Packet, BuildAndParse);
    RUN_TEST(Packet, SessionManagerPacket);
    RUN_TEST(Packet, ComPacketHeader);
    RUN_TEST(Packet, Padding);
    printf("  All Packet tests passed!\n\n");
}
#endif
