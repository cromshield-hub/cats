#include "libsed/ssc/pyrite/pyrite_session.h"
#include "libsed/core/uid.h"
#include "libsed/security/hash_password.h"

namespace libsed {

PyriteSession::PyriteSession(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport)
    , sessionManager_(transport, comId)
    , comId_(comId) {}

PyriteSession::~PyriteSession() {
    if (isActive()) close();
}

Result PyriteSession::openAdminSession(const std::string& sidPassword) {
    if (isActive()) close();

    Uid authority;
    Bytes credential;

    if (!sidPassword.empty()) {
        authority = Uid(uid::AUTH_SID);
        credential = HashPassword::passwordToBytes(sidPassword);
    }

    return sessionManager_.openSession(
        Uid(uid::SP_ADMIN), true, session_, authority, credential);
}

Result PyriteSession::openLockingSession(const std::string& password, uint32_t userId) {
    if (isActive()) close();

    Uid userAuth = uid::makeUserUid(userId);
    Bytes credential = HashPassword::passwordToBytes(password);

    return sessionManager_.openSession(
        Uid(uid::SP_LOCKING), true, session_, userAuth, credential);
}

Result PyriteSession::close() {
    if (!session_) return ErrorCode::Success;
    auto r = sessionManager_.closeSession(session_);
    session_.reset();
    return r;
}

} // namespace libsed
