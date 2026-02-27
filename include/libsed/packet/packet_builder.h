#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "com_packet.h"
#include <vector>

namespace libsed {

/// Builds the full ComPacket → Packet → SubPacket hierarchy
/// and parses responses back into token data
class PacketBuilder {
public:
    PacketBuilder() = default;

    /// Set session parameters
    void setComId(uint16_t comId, uint16_t extension = 0) {
        comId_ = comId;
        comIdExtension_ = extension;
    }

    void setSessionNumbers(uint32_t tsn, uint32_t hsn) {
        tsn_ = tsn;
        hsn_ = hsn;
    }

    void setSeqNumber(uint32_t seq) { seqNumber_ = seq; }

    // ── Building ─────────────────────────────────────

    /// Build a complete ComPacket from token payload
    Bytes buildComPacket(const Bytes& tokenPayload);

    /// Build ComPacket for session manager (TSN=0, HSN=0)
    Bytes buildSessionManagerPacket(const Bytes& tokenPayload);

    // ── Parsing ──────────────────────────────────────

    /// Parse a ComPacket response, extracting the token payload
    struct ParsedResponse {
        ComPacketHeader  comPacketHeader;
        PacketHeader     packetHeader;
        SubPacketHeader  subPacketHeader;
        Bytes            tokenPayload;
    };

    Result parseResponse(const uint8_t* data, size_t len, ParsedResponse& out);
    Result parseResponse(const Bytes& data, ParsedResponse& out) {
        return parseResponse(data.data(), data.size(), out);
    }

    /// Get outstanding data amount from last parse
    uint32_t outstandingData() const { return lastOutstandingData_; }

    /// Check if response indicates more data
    bool hasMoreData() const { return lastOutstandingData_ > 0; }

    // ── Padding ──────────────────────────────────────

    /// Pad buffer to 4-byte alignment (SubPacket padding requirement)
    static void padTo4(Bytes& buf);

    /// Calculate padded length
    static size_t paddedLength(size_t len) {
        return (len + 3) & ~static_cast<size_t>(3);
    }

private:
    uint16_t comId_ = 0;
    uint16_t comIdExtension_ = 0;
    uint32_t tsn_ = 0;
    uint32_t hsn_ = 0;
    uint32_t seqNumber_ = 0;
    uint32_t lastOutstandingData_ = 0;
};

} // namespace libsed
