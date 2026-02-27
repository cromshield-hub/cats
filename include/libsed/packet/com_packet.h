#pragma once

#include "../core/types.h"
#include "../core/endian.h"
#include "../core/error.h"
#include <vector>

namespace libsed {

/// ComPacket header structure (TCG Core Spec 3.2.2)
/// Total header size: 20 bytes
struct ComPacketHeader {
    uint32_t reserved = 0;        // 4 bytes (must be 0)
    uint16_t comId = 0;           // 2 bytes - ComID
    uint16_t comIdExtension = 0;  // 2 bytes - ComID Extension
    uint32_t outstandingData = 0; // 4 bytes
    uint32_t minTransfer = 0;     // 4 bytes
    uint32_t length = 0;          // 4 bytes - payload length

    static constexpr size_t HEADER_SIZE = 20;

    /// Serialize to bytes
    void serialize(std::vector<uint8_t>& buf) const;

    /// Deserialize from bytes (must have at least HEADER_SIZE bytes)
    static Result deserialize(const uint8_t* data, size_t len, ComPacketHeader& out);
};

/// Packet header structure (TCG Core Spec 3.2.3)
/// Total header size: 24 bytes
struct PacketHeader {
    uint32_t tperSessionNumber = 0;  // TSN
    uint32_t hostSessionNumber = 0;  // HSN
    uint32_t seqNumber = 0;
    uint16_t reserved = 0;
    uint16_t ackType = 0;
    uint32_t acknowledgement = 0;
    uint32_t length = 0;             // payload length

    static constexpr size_t HEADER_SIZE = 24;

    void serialize(std::vector<uint8_t>& buf) const;
    static Result deserialize(const uint8_t* data, size_t len, PacketHeader& out);
};

/// SubPacket header structure (TCG Core Spec 3.2.4)
/// Total header size: 12 bytes
struct SubPacketHeader {
    uint8_t  reserved[6] = {};
    uint16_t kind = 0;      // 0 = Data, 1 = Credit Control
    uint32_t length = 0;    // payload length

    static constexpr size_t HEADER_SIZE = 12;
    static constexpr uint16_t KIND_DATA = 0;
    static constexpr uint16_t KIND_CREDIT = 1;

    void serialize(std::vector<uint8_t>& buf) const;
    static Result deserialize(const uint8_t* data, size_t len, SubPacketHeader& out);
};

} // namespace libsed
