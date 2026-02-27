#include "mock/mock_transport.h"

namespace libsed {
namespace test {

MockTransport::MockTransport()
    : devicePath_("/dev/mock_sed") {}

Result MockTransport::ifSend(uint8_t protocolId, uint16_t comId, ByteSpan payload) {
    SendRecord rec;
    rec.protocolId = protocolId;
    rec.comId = comId;
    rec.data.assign(payload.begin(), payload.end());
    sendHistory_.push_back(std::move(rec));

    if (sendHandler_) {
        return sendHandler_(protocolId, comId, payload);
    }
    return ErrorCode::Success;
}

Result MockTransport::ifRecv(uint8_t protocolId, uint16_t comId,
                               MutableByteSpan buffer, size_t& bytesReceived) {
    if (recvHandler_) {
        return recvHandler_(protocolId, comId, buffer, bytesReceived);
    }

    if (recvQueue_.empty()) {
        bytesReceived = 0;
        return ErrorCode::Success;
    }

    const auto& response = recvQueue_.front();
    size_t copyLen = std::min(response.size(), buffer.size());
    std::memcpy(buffer.data(), response.data(), copyLen);
    bytesReceived = copyLen;

    recvQueue_.erase(recvQueue_.begin());
    return ErrorCode::Success;
}

void MockTransport::queueRecvData(const Bytes& data) {
    recvQueue_.push_back(data);
}

void MockTransport::queueDiscoveryResponse(SscType ssc) {
    // Build a minimal Level 0 Discovery response
    Bytes response(512, 0);

    // Header (48 bytes)
    uint32_t totalLen = 48 + 20 + 20 + 20; // header + TPer + Locking + SSC feature
    Endian::writeBe32(response.data(), totalLen - 4);
    Endian::writeBe16(response.data() + 4, 0); // major version
    Endian::writeBe16(response.data() + 6, 1); // minor version

    size_t offset = 48;

    // TPer feature (0x0001)
    Endian::writeBe16(response.data() + offset, 0x0001);
    response[offset + 2] = 0x10; // version 1
    response[offset + 3] = 16;   // data length
    response[offset + 4] = 0x01; // sync supported
    offset += 20;

    // Locking feature (0x0002)
    Endian::writeBe16(response.data() + offset, 0x0002);
    response[offset + 2] = 0x10;
    response[offset + 3] = 16;
    response[offset + 4] = 0x07; // supported, enabled, locked
    offset += 20;

    // SSC feature
    uint16_t featureCode = 0x0203; // Opal v2 default
    uint16_t baseComId = 0x0001;

    switch (ssc) {
        case SscType::Enterprise:
            featureCode = 0x0100;
            baseComId = 0x0001;
            break;
        case SscType::Pyrite10:
            featureCode = 0x0302;
            baseComId = 0x0001;
            break;
        case SscType::Pyrite20:
            featureCode = 0x0303;
            baseComId = 0x0001;
            break;
        default:
            break;
    }

    Endian::writeBe16(response.data() + offset, featureCode);
    response[offset + 2] = 0x10;
    response[offset + 3] = 16;
    Endian::writeBe16(response.data() + offset + 4, baseComId);
    Endian::writeBe16(response.data() + offset + 6, 1); // numComIds

    recvQueue_.push_back(response);
}

} // namespace test
} // namespace libsed
