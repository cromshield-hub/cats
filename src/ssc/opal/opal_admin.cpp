#include "libsed/ssc/opal/opal_admin.h"
#include "libsed/ssc/opal/opal_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"
#include "libsed/security/hash_password.h"

namespace libsed {

OpalAdmin::OpalAdmin(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result OpalAdmin::getMsidPin(Bytes& msidPin) {
    OpalSession session(transport_, comId_);

    // Open Admin SP as Anybody (no auth needed for MSID read)
    auto r = session.openAdminSession();
    if (r.failed()) return r;

    TableOps ops(*session.session());
    r = ops.getBytes(Uid(uid::CPIN_MSID), uid::col::PIN, msidPin);

    session.close();
    return r;
}

Result OpalAdmin::takeOwnership(const std::string& newSidPassword,
                                  const std::string& msidPassword) {
    Bytes msid;

    if (msidPassword.empty()) {
        // Auto-read MSID
        auto r = getMsidPin(msid);
        if (r.failed()) {
            LIBSED_ERROR("Failed to read MSID PIN");
            return r;
        }
    } else {
        msid = HashPassword::passwordToBytes(msidPassword);
    }

    // Open Admin SP session as SID using MSID credential
    SessionManager mgr(transport_, comId_);
    std::unique_ptr<Session> session;

    auto r = mgr.openSession(Uid(uid::SP_ADMIN), true, session,
                              Uid(uid::AUTH_SID), msid);
    if (r.failed()) {
        LIBSED_ERROR("Failed to open Admin SP with MSID");
        return r;
    }

    // Set new SID password
    TableOps ops(*session);
    Bytes newPin = HashPassword::passwordToBytes(newSidPassword);
    r = ops.setPin(Uid(uid::CPIN_SID), newPin);

    mgr.closeSession(session);

    if (r.ok()) {
        LIBSED_INFO("Ownership taken successfully");
    }
    return r;
}

Result OpalAdmin::activateLockingSP(const std::string& sidPassword) {
    OpalSession session(transport_, comId_);

    auto r = session.openAdminSession(sidPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    r = ops.activate(Uid(uid::SP_LOCKING));

    session.close();

    if (r.ok()) {
        LIBSED_INFO("Locking SP activated");
    }
    return r;
}

Result OpalAdmin::revertTPer(const std::string& sidPassword) {
    OpalSession session(transport_, comId_);

    auto r = session.openAdminSession(sidPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    r = ops.revertSP(Uid(uid::SP_ADMIN));
    // Note: session will be forcibly closed by TPer after revert

    LIBSED_INFO("TPer revert %s", r.ok() ? "succeeded" : "failed");
    return r;
}

Result OpalAdmin::revertLockingSP(const std::string& password, bool useSid) {
    if (useSid) {
        // Revert via Admin SP + SID
        OpalSession session(transport_, comId_);
        auto r = session.openAdminSession(password);
        if (r.failed()) return r;

        TableOps ops(*session.session());
        r = ops.revertSP(Uid(uid::SP_LOCKING));

        session.close();
        return r;
    } else {
        // Revert via Locking SP + Admin1
        OpalSession session(transport_, comId_);
        auto r = session.openLockingSessionAsAdmin(password);
        if (r.failed()) return r;

        TableOps ops(*session.session());
        r = ops.revertSP(Uid(uid::THIS_SP));

        return r;
    }
}

Result OpalAdmin::psidRevert(const std::string& psidPassword) {
    SessionManager mgr(transport_, comId_);
    std::unique_ptr<Session> session;

    Bytes credential = HashPassword::passwordToBytes(psidPassword);

    auto r = mgr.openSession(Uid(uid::SP_ADMIN), true, session,
                              Uid(uid::AUTH_PSID), credential);
    if (r.failed()) return r;

    TableOps ops(*session);
    r = ops.revertSP(Uid(uid::SP_ADMIN));

    LIBSED_INFO("PSID revert %s", r.ok() ? "succeeded" : "failed");
    return r;
}

Result OpalAdmin::setSidPassword(const std::string& currentPassword,
                                   const std::string& newPassword) {
    OpalSession session(transport_, comId_);

    auto r = session.openAdminSession(currentPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Bytes newPin = HashPassword::passwordToBytes(newPassword);
    r = ops.setPin(Uid(uid::CPIN_SID), newPin);

    session.close();
    return r;
}

} // namespace libsed
