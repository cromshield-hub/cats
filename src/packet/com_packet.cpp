#include "libsed/packet/com_packet.h"

namespace libsed {

// ══════════════════════════════════════════════════════
//  ComPacketHeader
// ══════════════════════════════════════════════════════
void ComPacketHeader::serialize(std::vector<uint8_t>& buf) const {
    Endian::appendBe32(buf, reserved);
    Endian::appendBe16(buf, comId);
    Endian::appendBe16(buf, comIdExtension);
    Endian::appendBe32(buf, outstandingData);
    Endian::appendBe32(buf, minTransfer);
    Endian::appendBe32(buf, length);
}

Result ComPacketHeader::deserialize(const uint8_t* data, size_t len,
                                      ComPacketHeader& out) {
    if (len < HEADER_SIZE) return ErrorCode::BufferTooSmall;

    out.reserved        = Endian::readBe32(data);
    out.comId           = Endian::readBe16(data + 4);
    out.comIdExtension  = Endian::readBe16(data + 6);
    out.outstandingData = Endian::readBe32(data + 8);
    out.minTransfer     = Endian::readBe32(data + 12);
    out.length          = Endian::readBe32(data + 16);

    return ErrorCode::Success;
}

// ══════════════════════════════════════════════════════
//  PacketHeader
// ══════════════════════════════════════════════════════
void PacketHeader::serialize(std::vector<uint8_t>& buf) const {
    Endian::appendBe32(buf, tperSessionNumber);
    Endian::appendBe32(buf, hostSessionNumber);
    Endian::appendBe32(buf, seqNumber);
    Endian::appendBe16(buf, reserved);
    Endian::appendBe16(buf, ackType);
    Endian::appendBe32(buf, acknowledgement);
    Endian::appendBe32(buf, length);
}

Result PacketHeader::deserialize(const uint8_t* data, size_t len,
                                   PacketHeader& out) {
    if (len < HEADER_SIZE) return ErrorCode::BufferTooSmall;

    out.tperSessionNumber = Endian::readBe32(data);
    out.hostSessionNumber = Endian::readBe32(data + 4);
    out.seqNumber         = Endian::readBe32(data + 8);
    out.reserved          = Endian::readBe16(data + 12);
    out.ackType           = Endian::readBe16(data + 14);
    out.acknowledgement   = Endian::readBe32(data + 16);
    out.length            = Endian::readBe32(data + 20);

    return ErrorCode::Success;
}

// ══════════════════════════════════════════════════════
//  SubPacketHeader
// ══════════════════════════════════════════════════════
void SubPacketHeader::serialize(std::vector<uint8_t>& buf) const {
    // 6 reserved bytes
    for (int i = 0; i < 6; ++i) buf.push_back(0);
    Endian::appendBe16(buf, kind);
    Endian::appendBe32(buf, length);
}

Result SubPacketHeader::deserialize(const uint8_t* data, size_t len,
                                      SubPacketHeader& out) {
    if (len < HEADER_SIZE) return ErrorCode::BufferTooSmall;

    // Skip 6 reserved bytes
    out.kind   = Endian::readBe16(data + 6);
    out.length = Endian::readBe32(data + 8);

    return ErrorCode::Success;
}

} // namespace libsed
