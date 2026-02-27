#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include "../../discovery/discovery.h"
#include "enterprise_defs.h"
#include "enterprise_auth.h"
#include "enterprise_band.h"
#include "enterprise_erase.h"
#include <memory>
#include <string>

namespace libsed {

/// High-level Enterprise device facade
class EnterpriseDevice {
public:
    EnterpriseDevice(std::shared_ptr<ITransport> transport,
                     uint16_t comId,
                     const DiscoveryInfo& info);

    const DiscoveryInfo& info() const { return info_; }

    EnterpriseAuth& auth() { return auth_; }
    EnterpriseBand& band() { return band_; }
    EnterpriseErase& erase() { return erase_; }

    // ── Convenience operations ───────────────────────

    /// Configure and lock a band
    Result setupBand(const std::string& bandMasterPassword,
                      uint32_t bandId,
                      uint64_t rangeStart, uint64_t rangeLength);

    /// Lock a band
    Result lockBand(const std::string& bandMasterPassword, uint32_t bandId) {
        return band_.lockBand(bandMasterPassword, bandId);
    }

    /// Unlock a band
    Result unlockBand(const std::string& bandMasterPassword, uint32_t bandId) {
        return band_.unlockBand(bandMasterPassword, bandId);
    }

    /// Crypto erase a band
    Result cryptoErase(const std::string& eraseMasterPassword, uint32_t bandId) {
        return erase_.cryptoErase(eraseMasterPassword, bandId);
    }

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
    DiscoveryInfo info_;

    EnterpriseAuth auth_;
    EnterpriseBand band_;
    EnterpriseErase erase_;
};

} // namespace libsed
