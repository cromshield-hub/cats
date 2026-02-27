#include "libsed/ssc/opal/opal_mbr.h"
#include "libsed/ssc/opal/opal_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"

namespace libsed {

OpalMbr::OpalMbr(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result OpalMbr::enableMbr(const std::string& admin1Password, bool enable) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(admin1Password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    r = ops.setBool(Uid(uid::MBRCTRL_SET), uid::col::MBR_ENABLE, enable);

    session.close();
    LIBSED_INFO("MBR %s: %s", enable ? "enabled" : "disabled",
                r.ok() ? "success" : "failed");
    return r;
}

Result OpalMbr::setMbrDone(const std::string& password, bool done,
                             uint32_t userId, bool asAdmin) {
    OpalSession session(transport_, comId_);
    Result r;

    if (asAdmin) {
        r = session.openLockingSessionAsAdmin(password);
    } else {
        r = session.openLockingSessionAsUser(userId, password);
    }
    if (r.failed()) return r;

    TableOps ops(*session.session());
    r = ops.setBool(Uid(uid::MBRCTRL_SET), uid::col::MBR_DONE, done);

    session.close();
    return r;
}

Result OpalMbr::writeMbrData(const std::string& admin1Password,
                               const uint8_t* data, size_t len,
                               uint64_t offset) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(admin1Password);
    if (r.failed()) return r;

    TableOps ops(*session.session());

    // Write to MBR table using Set method with Where clause for offset
    // The MBR table row UID is TABLE_MBR + 1
    Uid mbrUid(uid::TABLE_MBR + 1);
    Bytes mbrData(data, data + len);

    // For large MBR writes, we may need to chunk
    // Max token size limits how much we can write per Set
    constexpr size_t CHUNK_SIZE = 1024;
    size_t written = 0;

    while (written < len) {
        size_t chunkLen = std::min(CHUNK_SIZE, len - written);
        Bytes chunk(data + written, data + written + chunkLen);

        // Use table Set with row offset
        r = ops.setBytes(mbrUid, static_cast<uint32_t>(offset + written), chunk);
        if (r.failed()) break;

        written += chunkLen;
    }

    session.close();
    return r;
}

Result OpalMbr::readMbrData(const std::string& admin1Password,
                              Bytes& data, uint64_t offset,
                              uint64_t length) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(admin1Password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid mbrUid(uid::TABLE_MBR + 1);
    r = ops.getBytes(mbrUid, static_cast<uint32_t>(offset), data);

    session.close();
    return r;
}

Result OpalMbr::getMbrStatus(const std::string& password,
                               bool& enabled, bool& done) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(password);
    if (r.failed()) return r;

    TableOps ops(*session.session());

    uint64_t val = 0;
    r = ops.getUint(Uid(uid::MBRCTRL_SET), uid::col::MBR_ENABLE, val);
    if (r.ok()) enabled = (val != 0);

    r = ops.getUint(Uid(uid::MBRCTRL_SET), uid::col::MBR_DONE, val);
    if (r.ok()) done = (val != 0);

    session.close();
    return ErrorCode::Success;
}

} // namespace libsed
