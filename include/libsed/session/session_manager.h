#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../transport/i_transport.h"
#include "session.h"
#include <memory>
#include <functional>

namespace libsed {

/// Manages sessions including authentication workflows
class SessionManager {
public:
    explicit SessionManager(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Open a session, optionally authenticating
    /// Returns an active Session ready for method calls
    Result openSession(const Uid& spUid, bool write,
                       std::unique_ptr<Session>& session,
                       const Uid& authority = Uid(),
                       const Bytes& credential = {});

    /// Open session + authenticate in separate step
    Result openSessionAndAuthenticate(const Uid& spUid,
                                       const Uid& authority,
                                       const std::string& password,
                                       std::unique_ptr<Session>& session);

    /// Close a session
    Result closeSession(std::unique_ptr<Session>& session);

    /// Execute a method within a new session (auto open + close)
    using MethodFunc = std::function<Result(Session&)>;
    Result withSession(const Uid& spUid, bool write,
                       const Uid& authority, const Bytes& credential,
                       const MethodFunc& func);

    /// Exchange Properties with TPer (Level 0)
    Result exchangeProperties();

    /// Get TPer properties from last exchange
    uint32_t tperMaxComPacketSize() const { return tperMaxComPacketSize_; }
    uint32_t tperMaxPacketSize() const { return tperMaxPacketSize_; }

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;

    // TPer properties from Properties exchange
    uint32_t tperMaxComPacketSize_ = 2048;
    uint32_t tperMaxPacketSize_ = 2028;
    uint32_t tperMaxTokenSize_ = 1992;
};

} // namespace libsed
