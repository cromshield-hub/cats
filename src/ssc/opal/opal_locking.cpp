#include "libsed/ssc/opal/opal_locking.h"
#include "libsed/ssc/opal/opal_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/method/param_decoder.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"

namespace libsed {

OpalLocking::OpalLocking(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result OpalLocking::configureRange(const std::string& admin1Password,
                                     uint32_t rangeId,
                                     uint64_t rangeStart,
                                     uint64_t rangeLength,
                                     bool readLockEnabled,
                                     bool writeLockEnabled) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(admin1Password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    // Set range start/length
    r = ops.setUint(rangeUid, uid::col::RANGE_START, rangeStart);
    if (r.failed()) { session.close(); return r; }

    r = ops.setUint(rangeUid, uid::col::RANGE_LENGTH, rangeLength);
    if (r.failed()) { session.close(); return r; }

    // Enable read/write lock
    r = ops.setBool(rangeUid, uid::col::READ_LOCK_EN, readLockEnabled);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(rangeUid, uid::col::WRITE_LOCK_EN, writeLockEnabled);

    session.close();
    LIBSED_INFO("Range %u configured: start=%lu, length=%lu", rangeId, rangeStart, rangeLength);
    return r;
}

Result OpalLocking::setLockEnabled(const std::string& admin1Password,
                                     uint32_t rangeId,
                                     bool readLockEnabled,
                                     bool writeLockEnabled) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(admin1Password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    r = ops.setBool(rangeUid, uid::col::READ_LOCK_EN, readLockEnabled);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(rangeUid, uid::col::WRITE_LOCK_EN, writeLockEnabled);

    session.close();
    return r;
}

Result OpalLocking::lock(const std::string& userPassword,
                           uint32_t rangeId,
                           uint32_t userId,
                           bool readLock,
                           bool writeLock) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsUser(userId, userPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    if (readLock) {
        r = ops.setBool(rangeUid, uid::col::READ_LOCKED, true);
        if (r.failed()) { session.close(); return r; }
    }
    if (writeLock) {
        r = ops.setBool(rangeUid, uid::col::WRITE_LOCKED, true);
    }

    session.close();
    return r;
}

Result OpalLocking::unlock(const std::string& userPassword,
                             uint32_t rangeId,
                             uint32_t userId) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsUser(userId, userPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    r = ops.setBool(rangeUid, uid::col::READ_LOCKED, false);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(rangeUid, uid::col::WRITE_LOCKED, false);

    session.close();
    return r;
}

Result OpalLocking::getRangeInfo(const std::string& password,
                                   uint32_t rangeId,
                                   LockingRangeInfo& info,
                                   uint32_t userId) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsUser(userId, password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    ParamDecoder::ColumnValues values;
    r = ops.getAll(rangeUid, values);
    if (r.failed()) { session.close(); return r; }

    info.rangeId = rangeId;
    ParamDecoder::decodeLockingRange(values, info);

    session.close();
    return ErrorCode::Success;
}

Result OpalLocking::cryptoErase(const std::string& admin1Password,
                                  uint32_t rangeId) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsAdmin(admin1Password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);
    r = ops.genKey(rangeUid);

    session.close();
    LIBSED_INFO("Crypto erase range %u: %s", rangeId, r.ok() ? "success" : "failed");
    return r;
}

Result OpalLocking::lockGlobal(const std::string& password, uint32_t userId) {
    return lock(password, 0, userId);
}

Result OpalLocking::unlockGlobal(const std::string& password, uint32_t userId) {
    return unlock(password, 0, userId);
}

} // namespace libsed
