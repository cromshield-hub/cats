#include "libsed/ssc/enterprise/enterprise_device.h"
#include "libsed/core/log.h"

namespace libsed {

EnterpriseDevice::EnterpriseDevice(std::shared_ptr<ITransport> transport,
                                     uint16_t comId,
                                     const DiscoveryInfo& info)
    : transport_(transport)
    , comId_(comId)
    , info_(info)
    , auth_(transport, comId)
    , band_(transport, comId)
    , erase_(transport, comId) {
    LIBSED_INFO("EnterpriseDevice created, ComID=0x%04X", comId);
}

Result EnterpriseDevice::setupBand(const std::string& bandMasterPassword,
                                     uint32_t bandId,
                                     uint64_t rangeStart, uint64_t rangeLength) {
    auto r = band_.configureBand(bandMasterPassword, bandId, rangeStart, rangeLength);
    if (r.failed()) return r;

    r = band_.setLockOnReset(bandMasterPassword, bandId, true);
    if (r.failed()) {
        LIBSED_WARN("Failed to set lock-on-reset for band %u", bandId);
    }

    return ErrorCode::Success;
}

} // namespace libsed
