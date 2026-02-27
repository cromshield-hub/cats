#include "libsed/ssc/opal/opal_session.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"
#include "libsed/security/hash_password.h"

namespace libsed {

OpalSession::OpalSession(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport)
    , sessionManager_(transport, comId)
    , comId_(comId) {}

OpalSession::~OpalSession() {
    if (isActive()) close();
}

Result OpalSession::openAdminSession(const std::string& sidPassword) {
    if (isActive()) {
        auto r = close();
        if (r.failed()) return r;
    }

    Uid authority;
    Bytes credential;

    if (!sidPassword.empty()) {
        authority = Uid(uid::AUTH_SID);
        credential = HashPassword::passwordToBytes(sidPassword);
    }

    return sessionManager_.openSession(
        Uid(uid::SP_ADMIN), true, session_, authority, credential);
}

Result OpalSession::openLockingSessionAsAdmin(const std::string& admin1Password) {
    if (isActive()) close();

    Bytes credential = HashPassword::passwordToBytes(admin1Password);

    return sessionManager_.openSession(
        Uid(uid::SP_LOCKING), true, session_,
        Uid(uid::AUTH_ADMIN1), credential);
}

Result OpalSession::openLockingSessionAsUser(uint32_t userId,
                                               const std::string& userPassword) {
    if (isActive()) close();

    Uid userAuth = uid::makeUserUid(userId);
    Bytes credential = HashPassword::passwordToBytes(userPassword);

    return sessionManager_.openSession(
        Uid(uid::SP_LOCKING), true, session_, userAuth, credential);
}

Result OpalSession::close() {
    if (!session_) return ErrorCode::Success;

    auto r = sessionManager_.closeSession(session_);
    session_.reset();
    return r;
}

} // namespace libsed
