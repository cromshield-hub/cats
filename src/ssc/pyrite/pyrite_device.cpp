#include "libsed/ssc/pyrite/pyrite_device.h"
#include "libsed/ssc/pyrite/pyrite_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"
#include "libsed/security/hash_password.h"

namespace libsed {

PyriteDevice::PyriteDevice(std::shared_ptr<ITransport> transport,
                             uint16_t comId,
                             const DiscoveryInfo& info)
    : transport_(transport)
    , comId_(comId)
    , info_(info)
    , locking_(transport, comId) {
    LIBSED_INFO("PyriteDevice created, ComID=0x%04X", comId);
}

Result PyriteDevice::takeOwnership(const std::string& newSidPassword) {
    // Read MSID
    PyriteSession session(transport_, comId_);
    auto r = session.openAdminSession();
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Bytes msid;
    r = ops.getBytes(Uid(uid::CPIN_MSID), uid::col::PIN, msid);
    session.close();
    if (r.failed()) return r;

    // Open as SID with MSID credential
    SessionManager mgr(transport_, comId_);
    std::unique_ptr<Session> sess;
    r = mgr.openSession(Uid(uid::SP_ADMIN), true, sess,
                          Uid(uid::AUTH_SID), msid);
    if (r.failed()) return r;

    TableOps ops2(*sess);
    Bytes newPin = HashPassword::passwordToBytes(newSidPassword);
    r = ops2.setPin(Uid(uid::CPIN_SID), newPin);

    mgr.closeSession(sess);
    return r;
}

Result PyriteDevice::revert(const std::string& sidPassword) {
    PyriteSession session(transport_, comId_);
    auto r = session.openAdminSession(sidPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    r = ops.revertSP(Uid(uid::SP_ADMIN));

    return r;
}

} // namespace libsed
