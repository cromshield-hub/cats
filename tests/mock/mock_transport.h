#pragma once

#include "libsed/transport/i_transport.h"
#include "libsed/core/endian.h"
#include <vector>
#include <functional>
#include <cstring>

namespace libsed {
namespace test {

/// Mock transport for unit testing
class MockTransport : public ITransport {
public:
    MockTransport();

    Result ifSend(uint8_t protocolId, uint16_t comId, ByteSpan payload) override;
    Result ifRecv(uint8_t protocolId, uint16_t comId,
                  MutableByteSpan buffer, size_t& bytesReceived) override;

    TransportType type() const override { return TransportType::ATA; }
    std::string devicePath() const override { return devicePath_; }
    bool isOpen() const override { return true; }
    void close() override {}

    // ── Test helpers ─────────────────────────────────

    /// Queue data to be returned by next ifRecv call
    void queueRecvData(const Bytes& data);

    /// Queue a minimal Level 0 Discovery response
    void queueDiscoveryResponse(SscType ssc = SscType::Opal20);

    /// Record of a send call
    struct SendRecord {
        uint8_t protocolId;
        uint16_t comId;
        Bytes data;
    };

    /// Get send history
    const std::vector<SendRecord>& sendHistory() const { return sendHistory_; }

    /// Clear send history
    void clearHistory() { sendHistory_.clear(); }

    /// Set custom send handler
    using SendHandler = std::function<Result(uint8_t, uint16_t, ByteSpan)>;
    void setSendHandler(SendHandler handler) { sendHandler_ = std::move(handler); }

    /// Set custom recv handler
    using RecvHandler = std::function<Result(uint8_t, uint16_t, MutableByteSpan, size_t&)>;
    void setRecvHandler(RecvHandler handler) { recvHandler_ = std::move(handler); }

private:
    std::string devicePath_;
    std::vector<SendRecord> sendHistory_;
    std::vector<Bytes> recvQueue_;
    SendHandler sendHandler_;
    RecvHandler recvHandler_;
};

} // namespace test
} // namespace libsed
