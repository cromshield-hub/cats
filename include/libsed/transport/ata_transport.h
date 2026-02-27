#pragma once

#include "i_transport.h"
#include <string>

namespace libsed {

/// ATA transport via Trusted Send/Receive (ATA Security Protocol)
class AtaTransport : public ITransport {
public:
    explicit AtaTransport(const std::string& devicePath);
    ~AtaTransport() override;

    Result ifSend(uint8_t protocolId, uint16_t comId, ByteSpan payload) override;
    Result ifRecv(uint8_t protocolId, uint16_t comId,
                  MutableByteSpan buffer, size_t& bytesReceived) override;

    TransportType type() const override { return TransportType::ATA; }
    std::string devicePath() const override { return devicePath_; }
    bool isOpen() const override { return fd_ >= 0; }
    void close() override;

private:
    Result open();

    std::string devicePath_;
    int fd_ = -1;
};

} // namespace libsed
