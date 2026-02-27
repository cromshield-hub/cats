#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include <cstdint>
#include <memory>
#include <string>

namespace libsed {

/// Abstract transport interface for IF-SEND / IF-RECV
class ITransport {
public:
    virtual ~ITransport() = default;

    /// Send data via IF-SEND (Trusted Send)
    /// @param protocolId  Security Protocol number
    /// @param comId       ComID (specific to protocol)
    /// @param payload     Data to send
    virtual Result ifSend(uint8_t protocolId,
                          uint16_t comId,
                          ByteSpan payload) = 0;

    /// Receive data via IF-RECV (Trusted Receive)
    /// @param protocolId  Security Protocol number
    /// @param comId       ComID
    /// @param buffer      Buffer to receive into
    /// @param bytesReceived  Actual bytes received
    virtual Result ifRecv(uint8_t protocolId,
                          uint16_t comId,
                          MutableByteSpan buffer,
                          size_t& bytesReceived) = 0;

    /// Get the transport type
    virtual TransportType type() const = 0;

    /// Get the device path
    virtual std::string devicePath() const = 0;

    /// Check if device is open and valid
    virtual bool isOpen() const = 0;

    /// Close the transport
    virtual void close() = 0;

    // ── Convenience wrappers ─────────────────────────

    /// IF-RECV with auto-allocated buffer
    Result ifRecv(uint8_t protocolId, uint16_t comId,
                  Bytes& outBuffer, size_t maxSize = 65536) {
        outBuffer.resize(maxSize);
        size_t received = 0;
        auto result = ifRecv(protocolId, comId,
                             MutableByteSpan(outBuffer.data(), outBuffer.size()),
                             received);
        if (result.ok()) {
            outBuffer.resize(received);
        }
        return result;
    }
};

} // namespace libsed
