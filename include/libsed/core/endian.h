#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace libsed {

/// Big-endian encoding/decoding utilities for TCG SED protocol
class Endian {
public:
    static uint16_t readBe16(const uint8_t* p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }

    static uint32_t readBe32(const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8)  |
               static_cast<uint32_t>(p[3]);
    }

    static uint64_t readBe64(const uint8_t* p) {
        return (static_cast<uint64_t>(readBe32(p)) << 32) |
               static_cast<uint64_t>(readBe32(p + 4));
    }

    static void writeBe16(uint8_t* p, uint16_t val) {
        p[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
        p[1] = static_cast<uint8_t>(val & 0xFF);
    }

    static void writeBe32(uint8_t* p, uint32_t val) {
        p[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
        p[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
        p[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
        p[3] = static_cast<uint8_t>(val & 0xFF);
    }

    static void writeBe64(uint8_t* p, uint64_t val) {
        writeBe32(p, static_cast<uint32_t>(val >> 32));
        writeBe32(p + 4, static_cast<uint32_t>(val & 0xFFFFFFFF));
    }

    /// Append big-endian 16-bit to a byte vector
    static void appendBe16(std::vector<uint8_t>& buf, uint16_t val) {
        buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    /// Append big-endian 32-bit to a byte vector
    static void appendBe32(std::vector<uint8_t>& buf, uint32_t val) {
        buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    /// Append big-endian 64-bit to a byte vector
    static void appendBe64(std::vector<uint8_t>& buf, uint64_t val) {
        appendBe32(buf, static_cast<uint32_t>(val >> 32));
        appendBe32(buf, static_cast<uint32_t>(val & 0xFFFFFFFF));
    }

    /// Minimum number of bytes needed to represent the unsigned value
    static size_t minBytesUnsigned(uint64_t val) {
        if (val == 0) return 0;
        size_t n = 0;
        uint64_t v = val;
        while (v > 0) { v >>= 8; ++n; }
        return n;
    }

    /// Minimum number of bytes needed to represent the signed value
    static size_t minBytesSigned(int64_t val) {
        if (val == 0) return 0;
        if (val > 0) {
            size_t n = minBytesUnsigned(static_cast<uint64_t>(val));
            // Need extra byte if high bit is set (would look negative)
            uint64_t topByte = (static_cast<uint64_t>(val) >> ((n - 1) * 8)) & 0xFF;
            if (topByte & 0x80) ++n;
            return n;
        }
        // Negative
        size_t n = 1;
        int64_t v = val;
        while (v < -128 || v > 127) { v >>= 8; ++n; }
        return n;
    }

    /// Encode unsigned value to big-endian bytes (variable length)
    static void encodeUnsigned(std::vector<uint8_t>& buf, uint64_t val, size_t nBytes) {
        for (int i = static_cast<int>(nBytes) - 1; i >= 0; --i) {
            buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }

    /// Encode signed value to big-endian bytes (variable length)
    static void encodeSigned(std::vector<uint8_t>& buf, int64_t val, size_t nBytes) {
        encodeUnsigned(buf, static_cast<uint64_t>(val), nBytes);
    }

    /// Decode unsigned value from big-endian bytes
    static uint64_t decodeUnsigned(const uint8_t* p, size_t nBytes) {
        uint64_t val = 0;
        for (size_t i = 0; i < nBytes; ++i) {
            val = (val << 8) | p[i];
        }
        return val;
    }

    /// Decode signed value from big-endian bytes
    static int64_t decodeSigned(const uint8_t* p, size_t nBytes) {
        if (nBytes == 0) return 0;
        int64_t val = (p[0] & 0x80) ? -1 : 0; // sign-extend
        for (size_t i = 0; i < nBytes; ++i) {
            val = (val << 8) | p[i];
        }
        return val;
    }
};

} // namespace libsed
