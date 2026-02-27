#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../session/session.h"
#include "../../session/session_manager.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Opal-specific session management
class OpalSession {
public:
    OpalSession(std::shared_ptr<ITransport> transport, uint16_t comId);
    ~OpalSession();

    /// Open Admin SP session (for SID/PSID/MSID operations)
    Result openAdminSession(const std::string& sidPassword = "");

    /// Open Locking SP session with Admin1
    Result openLockingSessionAsAdmin(const std::string& admin1Password);

    /// Open Locking SP session with a User
    Result openLockingSessionAsUser(uint32_t userId, const std::string& userPassword);

    /// Close current session
    Result close();

    /// Get underlying session
    Session* session() { return session_.get(); }
    bool isActive() const { return session_ && session_->isActive(); }

private:
    std::shared_ptr<ITransport> transport_;
    SessionManager sessionManager_;
    std::unique_ptr<Session> session_;
    uint16_t comId_;
};

} // namespace libsed
