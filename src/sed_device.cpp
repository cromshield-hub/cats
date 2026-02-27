#include "libsed/sed_device.h"
#include "libsed/transport/transport_factory.h"
#include "libsed/core/log.h"

namespace libsed {

SedDevice::~SedDevice() = default;

std::unique_ptr<SedDevice> SedDevice::open(const std::string& devicePath) {
    auto transport = TransportFactory::create(devicePath);
    if (!transport) {
        LIBSED_ERROR("Failed to create transport for %s", devicePath.c_str());
        return nullptr;
    }
    return open(transport);
}

std::unique_ptr<SedDevice> SedDevice::open(std::shared_ptr<ITransport> transport) {
    auto device = std::unique_ptr<SedDevice>(new SedDevice());
    device->transport_ = transport;

    auto r = device->initialize();
    if (r.failed()) {
        LIBSED_ERROR("Failed to initialize SedDevice");
        return nullptr;
    }

    return device;
}

Result SedDevice::initialize() {
    // Perform Level 0 Discovery
    auto r = discoveryParser_.discover(transport_);
    if (r.failed()) return r;

    discoveryInfo_ = discoveryParser_.buildInfo();
    uint16_t comId = discoveryInfo_.baseComId;

    LIBSED_INFO("Detected SSC: %d, ComID: 0x%04X",
                static_cast<int>(discoveryInfo_.primarySsc), comId);

    // Create SSC-specific device
    switch (discoveryInfo_.primarySsc) {
        case SscType::Opal20:
        case SscType::Opal10:
            opalDevice_ = std::make_unique<OpalDevice>(transport_, comId, discoveryInfo_);
            break;
        case SscType::Enterprise:
            enterpriseDevice_ = std::make_unique<EnterpriseDevice>(transport_, comId, discoveryInfo_);
            break;
        case SscType::Pyrite10:
        case SscType::Pyrite20:
            pyriteDevice_ = std::make_unique<PyriteDevice>(transport_, comId, discoveryInfo_);
            break;
        default:
            LIBSED_WARN("Unknown SSC type, limited functionality");
            break;
    }

    return ErrorCode::Success;
}

Result SedDevice::rediscover() {
    opalDevice_.reset();
    enterpriseDevice_.reset();
    pyriteDevice_.reset();
    return initialize();
}

// ── Common operations ────────────────────────────────

Result SedDevice::takeOwnership(const std::string& newSidPassword) {
    if (opalDevice_)
        return opalDevice_->takeOwnership(newSidPassword);
    if (pyriteDevice_)
        return pyriteDevice_->takeOwnership(newSidPassword);
    // Enterprise doesn't have a "take ownership" in the same sense
    return ErrorCode::UnsupportedSsc;
}

Result SedDevice::revert(const std::string& password) {
    if (opalDevice_)
        return opalDevice_->revertTPer(password);
    if (pyriteDevice_)
        return pyriteDevice_->revert(password);
    return ErrorCode::UnsupportedSsc;
}

Result SedDevice::lockRange(uint32_t rangeId, const std::string& password,
                              uint32_t authId) {
    if (opalDevice_)
        return opalDevice_->lock(password, rangeId, authId);
    if (enterpriseDevice_)
        return enterpriseDevice_->lockBand(password, rangeId);
    if (pyriteDevice_)
        return pyriteDevice_->lock(password, authId);
    return ErrorCode::UnsupportedSsc;
}

Result SedDevice::unlockRange(uint32_t rangeId, const std::string& password,
                                uint32_t authId) {
    if (opalDevice_)
        return opalDevice_->unlock(password, rangeId, authId);
    if (enterpriseDevice_)
        return enterpriseDevice_->unlockBand(password, rangeId);
    if (pyriteDevice_)
        return pyriteDevice_->unlock(password, authId);
    return ErrorCode::UnsupportedSsc;
}

Result SedDevice::configureRange(uint32_t rangeId,
                                   uint64_t rangeStart, uint64_t rangeLength,
                                   const std::string& adminPassword) {
    if (opalDevice_)
        return opalDevice_->setupRange(adminPassword, rangeId, rangeStart, rangeLength);
    if (enterpriseDevice_)
        return enterpriseDevice_->setupBand(adminPassword, rangeId, rangeStart, rangeLength);
    return ErrorCode::UnsupportedSsc;
}

Result SedDevice::getRangeInfo(uint32_t rangeId, LockingRangeInfo& info,
                                 const std::string& password, uint32_t authId) {
    if (opalDevice_)
        return opalDevice_->locking().getRangeInfo(password, rangeId, info, authId);
    if (pyriteDevice_)
        return pyriteDevice_->locking().getRangeInfo(password, rangeId, info, authId);
    return ErrorCode::UnsupportedSsc;
}

} // namespace libsed
