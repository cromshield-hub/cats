#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include "enterprise_defs.h"
#include <memory>
#include <string>

namespace libsed {

/// Enterprise band management
class EnterpriseBand {
public:
    EnterpriseBand(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Configure a band's range
    Result configureBand(const std::string& bandMasterPassword,
                          uint32_t bandId,
                          uint64_t rangeStart,
                          uint64_t rangeLength);

    /// Lock a band
    Result lockBand(const std::string& bandMasterPassword, uint32_t bandId);

    /// Unlock a band
    Result unlockBand(const std::string& bandMasterPassword, uint32_t bandId);

    /// Get band information
    Result getBandInfo(const std::string& bandMasterPassword,
                        uint32_t bandId,
                        enterprise::BandInfo& info);

    /// Set lock-on-reset for a band
    Result setLockOnReset(const std::string& bandMasterPassword,
                           uint32_t bandId, bool lockOnReset);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
