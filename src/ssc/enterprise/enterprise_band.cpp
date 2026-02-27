#include "libsed/ssc/enterprise/enterprise_band.h"
#include "libsed/ssc/enterprise/enterprise_session.h"
#include "libsed/table/table_ops.h"
#include "libsed/method/param_decoder.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"

namespace libsed {

EnterpriseBand::EnterpriseBand(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(transport), comId_(comId) {}

Result EnterpriseBand::configureBand(const std::string& bandMasterPassword,
                                       uint32_t bandId,
                                       uint64_t rangeStart,
                                       uint64_t rangeLength) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, bandMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);

    r = ops.setUint(bandUid, uid::col::RANGE_START, rangeStart);
    if (r.failed()) { session.close(); return r; }

    r = ops.setUint(bandUid, uid::col::RANGE_LENGTH, rangeLength);

    session.close();
    return r;
}

Result EnterpriseBand::lockBand(const std::string& bandMasterPassword, uint32_t bandId) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, bandMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);

    r = ops.setBool(bandUid, uid::col::READ_LOCKED, true);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(bandUid, uid::col::WRITE_LOCKED, true);

    session.close();
    return r;
}

Result EnterpriseBand::unlockBand(const std::string& bandMasterPassword, uint32_t bandId) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, bandMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);

    r = ops.setBool(bandUid, uid::col::READ_LOCKED, false);
    if (r.failed()) { session.close(); return r; }

    r = ops.setBool(bandUid, uid::col::WRITE_LOCKED, false);

    session.close();
    return r;
}

Result EnterpriseBand::getBandInfo(const std::string& bandMasterPassword,
                                     uint32_t bandId,
                                     enterprise::BandInfo& info) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, bandMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);

    ParamDecoder::ColumnValues values;
    r = ops.getAll(bandUid, values);
    if (r.ok()) {
        info.bandId = bandId;
        auto start = ParamDecoder::extractUint(values, uid::col::RANGE_START);
        if (start) info.rangeStart = *start;
        auto len = ParamDecoder::extractUint(values, uid::col::RANGE_LENGTH);
        if (len) info.rangeLength = *len;
        auto locked = ParamDecoder::extractBool(values, uid::col::READ_LOCKED);
        if (locked) info.locked = *locked;
    }

    session.close();
    return r;
}

Result EnterpriseBand::setLockOnReset(const std::string& bandMasterPassword,
                                        uint32_t bandId, bool lockOnReset) {
    EnterpriseSession session(transport_, comId_);
    auto r = session.openAsBandMaster(bandId, bandMasterPassword);
    if (r.failed()) return r;

    TableOps ops(*session.session());
    Uid bandUid = enterprise::makeBandUid(bandId);

    // LockOnReset is typically column 9
    r = ops.setBool(bandUid, 9, lockOnReset);

    session.close();
    return r;
}

} // namespace libsed
