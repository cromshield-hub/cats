#include "libsed/ssc/pyrite/pyrite_locking.h"
#include "libsed/ssc/pyrite/pyrite_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/method/param_decoder.h"
#include "libsed/core/uid.h"

namespace libsed {

PyriteLocking::PyriteLocking(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result PyriteLocking::lock(const std::string& userPassword,
                             uint32_t rangeId, uint32_t userId) {
    PyriteSession session(transport_, comId_);
    auto r = session.openLockingSession(userPassword, userId);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    r = ops.setBool(rangeUid, uid::col::READ_LOCKED, true);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(rangeUid, uid::col::WRITE_LOCKED, true);

    session.close();
    return r;
}

Result PyriteLocking::unlock(const std::string& userPassword,
                               uint32_t rangeId, uint32_t userId) {
    PyriteSession session(transport_, comId_);
    auto r = session.openLockingSession(userPassword, userId);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    r = ops.setBool(rangeUid, uid::col::READ_LOCKED, false);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(rangeUid, uid::col::WRITE_LOCKED, false);

    session.close();
    return r;
}

Result PyriteLocking::getRangeInfo(const std::string& password,
                                     uint32_t rangeId,
                                     LockingRangeInfo& info,
                                     uint32_t userId) {
    PyriteSession session(transport_, comId_);
    auto r = session.openLockingSession(password, userId);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);

    ParamDecoder::ColumnValues values;
    r = ops.getAll(rangeUid, values);
    if (r.ok()) {
        info.rangeId = rangeId;
        ParamDecoder::decodeLockingRange(values, info);
    }

    session.close();
    return r;
}

} // namespace libsed
