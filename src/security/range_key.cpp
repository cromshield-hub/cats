#include "libsed/security/range_key.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"

namespace libsed {

Result RangeKey::generateKey(uint32_t rangeId) {
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);
    TableOps ops(session_);
    return ops.genKey(rangeUid);
}

Result RangeKey::getActiveKey(uint32_t rangeId, Uid& keyUid) {
    Uid rangeUid = uid::makeLockingRangeUid(rangeId);
    TableOps ops(session_);

    Bytes keyBytes;
    auto r = ops.getBytes(rangeUid, uid::col::ACTIVE_KEY, keyBytes);
    if (r.failed()) return r;

    if (keyBytes.size() == 8) {
        std::copy(keyBytes.begin(), keyBytes.end(), keyUid.bytes.begin());
    }

    return ErrorCode::Success;
}

} // namespace libsed
