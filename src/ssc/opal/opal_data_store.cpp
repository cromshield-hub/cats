#include "libsed/ssc/opal/opal_data_store.h"
#include "libsed/ssc/opal/opal_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"

namespace libsed {

OpalDataStore::OpalDataStore(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result OpalDataStore::write(const std::string& password,
                              const uint8_t* data, size_t len,
                              uint32_t tableNumber,
                              uint64_t offset,
                              uint32_t userId) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsUser(userId, password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid dsUid(uid::TABLE_DATASTORE + tableNumber + 1);

    Bytes chunk(data, data + len);
    r = ops.setBytes(dsUid, static_cast<uint32_t>(offset), chunk);

    session.close();
    return r;
}

Result OpalDataStore::read(const std::string& password,
                             Bytes& data,
                             uint32_t tableNumber,
                             uint64_t offset,
                             uint64_t /*length*/,
                             uint32_t userId) {
    OpalSession session(transport_, comId_);
    auto r = session.openLockingSessionAsUser(userId, password);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid dsUid(uid::TABLE_DATASTORE + tableNumber + 1);

    r = ops.getBytes(dsUid, static_cast<uint32_t>(offset), data);

    session.close();
    return r;
}

} // namespace libsed
