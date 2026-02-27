#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Opal Shadow MBR control
class OpalMbr {
public:
    OpalMbr(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Enable Shadow MBR (requires Admin1)
    Result enableMbr(const std::string& admin1Password, bool enable);

    /// Set MBR Done flag
    Result setMbrDone(const std::string& password, bool done,
                       uint32_t userId = 1, bool asAdmin = false);

    /// Write data to Shadow MBR table
    Result writeMbrData(const std::string& admin1Password,
                         const uint8_t* data, size_t len,
                         uint64_t offset = 0);

    /// Read data from Shadow MBR table
    Result readMbrData(const std::string& admin1Password,
                        Bytes& data, uint64_t offset = 0,
                        uint64_t length = 0);

    /// Get MBR status
    Result getMbrStatus(const std::string& password,
                         bool& enabled, bool& done);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
