#pragma once

#include "i_transport.h"
#include <string>

namespace libsed {

/// SCSI transport via Security Protocol In/Out commands
class ScsiTransport : public ITransport {
public:
    explicit ScsiTransport(const std::string& devicePath);
    ~ScsiTransport() override;

    Result ifSend(uint8_t protocolId, uint16_t comId, ByteSpan payload) override;
    Result ifRecv(uint8_t protocolId, uint16_t comId,
                  MutableByteSpan buffer, size_t& bytesReceived) override;

    TransportType type() const override { return TransportType::SCSI; }
    std::string devicePath() const override { return devicePath_; }
    bool isOpen() const override { return fd_ >= 0; }
    void close() override;

private:
    Result open();

    std::string devicePath_;
    int fd_ = -1;
};

} // namespace libsed
