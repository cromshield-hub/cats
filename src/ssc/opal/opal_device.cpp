#include "libsed/ssc/opal/opal_device.h"
#include "libsed/core/log.h"

namespace libsed {

OpalDevice::OpalDevice(std::shared_ptr<ITransport> transport,
                         uint16_t comId,
                         const DiscoveryInfo& info)
    : transport_(transport)
    , comId_(comId)
    , info_(info)
    , admin_(transport, comId)
    , locking_(transport, comId)
    , user_(transport, comId)
    , mbr_(transport, comId)
    , dataStore_(transport, comId) {
    LIBSED_INFO("OpalDevice created, ComID=0x%04X", comId);
}

Result OpalDevice::initialSetup(const std::string& sidPassword,
                                  const std::string& admin1Password) {
    // 1. Take ownership
    auto r = admin_.takeOwnership(sidPassword);
    if (r.failed()) {
        LIBSED_ERROR("Take ownership failed");
        return r;
    }

    // 2. Activate Locking SP
    r = admin_.activateLockingSP(sidPassword);
    if (r.failed()) {
        LIBSED_ERROR("Activate Locking SP failed");
        return r;
    }

    // 3. Set Admin1 password on Locking SP
    r = user_.setAdmin1Password(sidPassword, admin1Password);
    if (r.failed()) {
        LIBSED_ERROR("Set Admin1 password failed");
        return r;
    }

    // 4. Enable global locking
    r = locking_.setLockEnabled(admin1Password, 0, true, true);
    if (r.failed()) {
        LIBSED_WARN("Enable global lock failed (may already be configured)");
    }

    LIBSED_INFO("Initial Opal setup complete");
    return ErrorCode::Success;
}

} // namespace libsed
