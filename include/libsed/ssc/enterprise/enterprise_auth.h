#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Enterprise authentication operations
class EnterpriseAuth {
public:
    EnterpriseAuth(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Set BandMaster password
    Result setBandMasterPassword(const std::string& currentPassword,
                                   const std::string& newPassword,
                                   uint32_t bandId);

    /// Set EraseMaster password
    Result setEraseMasterPassword(const std::string& currentPassword,
                                    const std::string& newPassword);

    /// Verify BandMaster credentials
    Result verifyBandMaster(const std::string& password, uint32_t bandId);

    /// Verify EraseMaster credentials
    Result verifyEraseMaster(const std::string& password);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
