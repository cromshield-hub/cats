#include "libsed/codec/token_encoder.h"
#include "libsed/codec/token_decoder.h"
#include "libsed/codec/token_stream.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
// Using Google Test
#else
#include <cassert>
#include <cstdio>
#endif

// Portable test macros
#ifndef TEST
#define TEST(suite, name) void test_##suite##_##name()
#define EXPECT_EQ(a, b) assert((a) == (b))
#define EXPECT_TRUE(a) assert(a)
#define EXPECT_FALSE(a) assert(!(a))
#define RUN_TEST(suite, name) do { printf("  " #suite "." #name "..."); test_##suite##_##name(); printf(" OK\n"); } while(0)
#endif

using namespace libsed;

TEST(TokenCodec, TinyAtomUnsigned) {
    TokenEncoder enc;
    enc.encodeUint(0);
    enc.encodeUint(42);
    enc.encodeUint(63);

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 3u);
    EXPECT_EQ(dec[0].getUint(), 0u);
    EXPECT_EQ(dec[1].getUint(), 42u);
    EXPECT_EQ(dec[2].getUint(), 63u);
}

TEST(TokenCodec, ShortAtomUnsigned) {
    TokenEncoder enc;
    enc.encodeUint(64);
    enc.encodeUint(255);
    enc.encodeUint(65535);

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 3u);
    EXPECT_EQ(dec[0].getUint(), 64u);
    EXPECT_EQ(dec[1].getUint(), 255u);
    EXPECT_EQ(dec[2].getUint(), 65535u);
}

TEST(TokenCodec, LargeUnsigned) {
    TokenEncoder enc;
    enc.encodeUint(0x100000000ULL); // > 32-bit

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 1u);
    EXPECT_EQ(dec[0].getUint(), 0x100000000ULL);
}

TEST(TokenCodec, ByteSequence) {
    TokenEncoder enc;
    Bytes data = {0x01, 0x02, 0x03, 0x04, 0x05};
    enc.encodeBytes(data);

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 1u);
    EXPECT_TRUE(dec[0].isByteSequence);
    EXPECT_EQ(dec[0].getBytes().size(), 5u);
    EXPECT_EQ(dec[0].getBytes()[0], 0x01);
}

TEST(TokenCodec, EmptyByteSequence) {
    TokenEncoder enc;
    enc.encodeBytes(Bytes{});

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 1u);
    EXPECT_TRUE(dec[0].isByteSequence);
    EXPECT_EQ(dec[0].getBytes().size(), 0u);
}

TEST(TokenCodec, ControlTokens) {
    TokenEncoder enc;
    enc.startList();
    enc.encodeUint(1);
    enc.encodeUint(2);
    enc.endList();
    enc.endOfData();

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 5u);
    EXPECT_EQ(dec[0].type, TokenType::StartList);
    EXPECT_EQ(dec[3].type, TokenType::EndList);
    EXPECT_EQ(dec[4].type, TokenType::EndOfData);
}

TEST(TokenCodec, NamedValues) {
    TokenEncoder enc;
    enc.startList();
    enc.namedUint(3, 100);
    enc.namedBool(5, true);
    enc.endList();

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());

    TokenStream stream(dec.releaseTokens());
    EXPECT_TRUE(stream.expectStartList());

    // Read named uint
    EXPECT_TRUE(stream.expectStartName());
    auto name1 = stream.readUint();
    EXPECT_TRUE(name1.has_value());
    EXPECT_EQ(*name1, 3u);
    auto val1 = stream.readUint();
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 100u);
    EXPECT_TRUE(stream.expectEndName());

    // Read named bool
    EXPECT_TRUE(stream.expectStartName());
    auto name2 = stream.readUint();
    EXPECT_EQ(*name2, 5u);
    auto val2 = stream.readBool();
    EXPECT_TRUE(val2.has_value());
    EXPECT_TRUE(*val2);
    EXPECT_TRUE(stream.expectEndName());
}

TEST(TokenCodec, UidEncoding) {
    TokenEncoder enc;
    Uid uid(0x0000000900030001ULL);
    enc.encodeUid(uid);

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 1u);
    EXPECT_TRUE(dec[0].isByteSequence);
    EXPECT_EQ(dec[0].getBytes().size(), 8u);

    TokenStream stream(dec.releaseTokens());
    auto readUid = stream.readUid();
    EXPECT_TRUE(readUid.has_value());
    EXPECT_EQ(readUid->toUint64(), 0x0000000900030001ULL);
}

TEST(TokenCodec, MediumAtomBytes) {
    // Create data larger than 15 bytes (requires medium atom)
    Bytes data(100, 0xAB);
    TokenEncoder enc;
    enc.encodeBytes(data);

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dec.count(), 1u);
    EXPECT_EQ(dec[0].getBytes().size(), 100u);
    EXPECT_EQ(dec[0].getBytes()[50], 0xAB);
}

TEST(TokenCodec, Roundtrip) {
    TokenEncoder enc;
    enc.call();
    enc.encodeBytes(Bytes{0,0,0,0,0,0,0,0xFF}); // SMUID
    enc.encodeBytes(Bytes{0,0,0,0,0,0,0xFF,2}); // StartSession method
    enc.startList();
    enc.encodeUint(1);
    enc.encodeBytes(Bytes{0,0,2,5,0,0,0,1}); // SP_ADMIN
    enc.encodeBool(true);
    enc.endList();
    enc.endOfData();
    enc.startList();
    enc.encodeUint(0);
    enc.encodeUint(0);
    enc.encodeUint(0);
    enc.endList();

    TokenDecoder dec;
    auto r = dec.decode(enc.data());
    EXPECT_TRUE(r.ok());
    // Call + 2 UIDs + StartList + 3 atoms + EndList + EOD + StartList + 3 atoms + EndList
    EXPECT_EQ(dec.count(), 14u);
}

#ifndef GTEST_INCLUDE_GTEST_GTEST_H_
// Standalone runner
void run_token_codec_tests() {
    printf("TokenCodec tests:\n");
    RUN_TEST(TokenCodec, TinyAtomUnsigned);
    RUN_TEST(TokenCodec, ShortAtomUnsigned);
    RUN_TEST(TokenCodec, LargeUnsigned);
    RUN_TEST(TokenCodec, ByteSequence);
    RUN_TEST(TokenCodec, EmptyByteSequence);
    RUN_TEST(TokenCodec, ControlTokens);
    RUN_TEST(TokenCodec, NamedValues);
    RUN_TEST(TokenCodec, UidEncoding);
    RUN_TEST(TokenCodec, MediumAtomBytes);
    RUN_TEST(TokenCodec, Roundtrip);
    printf("  All TokenCodec tests passed!\n\n");
}
#endif
