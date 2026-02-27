#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Opal administrative operations
class OpalAdmin {
public:
    OpalAdmin(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Take ownership: set SID password (requires MSID)
    Result takeOwnership(const std::string& newSidPassword,
                          const std::string& msidPassword = "");

    /// Get MSID PIN (from Admin SP, Anybody authority)
    Result getMsidPin(Bytes& msidPin);

    /// Activate Locking SP (requires Admin SP session)
    Result activateLockingSP(const std::string& sidPassword);

    /// Revert TPer to factory state (using SID)
    Result revertTPer(const std::string& sidPassword);

    /// Revert Locking SP (using Admin1 on Locking SP, or SID on Admin SP)
    Result revertLockingSP(const std::string& password, bool useSid = false);

    /// PSID Revert (Physical Presence)
    Result psidRevert(const std::string& psidPassword);

    /// Set Admin SP SID password
    Result setSidPassword(const std::string& currentPassword,
                           const std::string& newPassword);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
