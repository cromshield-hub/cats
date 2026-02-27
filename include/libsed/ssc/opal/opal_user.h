#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Opal user/authority management
class OpalUser {
public:
    OpalUser(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Enable a user authority in Locking SP (requires Admin1)
    Result enableUser(const std::string& admin1Password, uint32_t userId);

    /// Disable a user authority
    Result disableUser(const std::string& admin1Password, uint32_t userId);

    /// Set user password (requires Admin1 or the user themselves)
    Result setUserPassword(const std::string& authPassword,
                            uint32_t userId,
                            const std::string& newPassword,
                            bool asAdmin = true);

    /// Set Admin1 password in Locking SP
    Result setAdmin1Password(const std::string& sidPassword,
                              const std::string& newPassword);

    /// Assign a user to a locking range (ACE manipulation)
    Result assignUserToRange(const std::string& admin1Password,
                              uint32_t userId,
                              uint32_t rangeId);

    /// Check if user is enabled
    Result isUserEnabled(const std::string& admin1Password,
                          uint32_t userId, bool& enabled);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
