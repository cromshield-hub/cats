#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../session/session.h"
#include "../../session/session_manager.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Pyrite SSC session management
class PyriteSession {
public:
    PyriteSession(std::shared_ptr<ITransport> transport, uint16_t comId);
    ~PyriteSession();

    /// Open Admin SP session
    Result openAdminSession(const std::string& sidPassword = "");

    /// Open Locking SP session
    Result openLockingSession(const std::string& password, uint32_t userId = 1);

    /// Close current session
    Result close();

    Session* session() { return session_.get(); }
    bool isActive() const { return session_ && session_->isActive(); }

private:
    std::shared_ptr<ITransport> transport_;
    SessionManager sessionManager_;
    std::unique_ptr<Session> session_;
    uint16_t comId_;
};

} // namespace libsed
