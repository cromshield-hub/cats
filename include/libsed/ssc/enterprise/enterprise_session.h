#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../session/session.h"
#include "../../session/session_manager.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Enterprise SSC session management
class EnterpriseSession {
public:
    EnterpriseSession(std::shared_ptr<ITransport> transport, uint16_t comId);
    ~EnterpriseSession();

    /// Open session to Enterprise SP as BandMaster
    Result openAsBandMaster(uint32_t bandId, const std::string& password);

    /// Open session to Enterprise SP as EraseMaster
    Result openAsEraseMaster(const std::string& password);

    /// Open session as Anybody (read-only)
    Result openAsAnybody();

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
