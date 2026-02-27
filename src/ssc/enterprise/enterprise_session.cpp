#include "libsed/ssc/enterprise/enterprise_session.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"
#include "libsed/security/hash_password.h"

namespace libsed {

EnterpriseSession::EnterpriseSession(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport)
    , sessionManager_(transport, comId)
    , comId_(comId) {}

EnterpriseSession::~EnterpriseSession() {
    if (isActive()) close();
}

Result EnterpriseSession::openAsBandMaster(uint32_t bandId, const std::string& password) {
    if (isActive()) close();

    Uid authority = uid::makeBandMasterUid(bandId);
    Bytes credential = HashPassword::passwordToBytes(password);

    return sessionManager_.openSession(
        Uid(uid::SP_ENTERPRISE), true, session_, authority, credential);
}

Result EnterpriseSession::openAsEraseMaster(const std::string& password) {
    if (isActive()) close();

    Bytes credential = HashPassword::passwordToBytes(password);

    return sessionManager_.openSession(
        Uid(uid::SP_ENTERPRISE), true, session_,
        Uid(uid::AUTH_ERASEMASTER), credential);
}

Result EnterpriseSession::openAsAnybody() {
    if (isActive()) close();

    return sessionManager_.openSession(
        Uid(uid::SP_ENTERPRISE), false, session_);
}

Result EnterpriseSession::close() {
    if (!session_) return ErrorCode::Success;
    auto r = sessionManager_.closeSession(session_);
    session_.reset();
    return r;
}

} // namespace libsed
