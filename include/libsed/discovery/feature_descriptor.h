#pragma once

#include "../core/types.h"
#include <cstdint>
#include <string>

namespace libsed {

/// Base class for Level 0 Feature Descriptors
class FeatureDescriptor {
public:
    virtual ~FeatureDescriptor() = default;

    uint16_t featureCode() const { return featureCode_; }
    uint8_t  version() const { return version_; }
    uint16_t dataLength() const { return dataLength_; }

    virtual std::string name() const = 0;
    virtual void parse(const uint8_t* data, size_t len) = 0;

protected:
    uint16_t featureCode_ = 0;
    uint8_t  version_ = 0;
    uint16_t dataLength_ = 0;

    void parseHeader(const uint8_t* data) {
        featureCode_ = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        version_ = (data[2] >> 4) & 0x0F;
        dataLength_ = static_cast<uint16_t>(data[3]);
    }
};

/// TPer Feature (0x0001)
class TPerFeature : public FeatureDescriptor {
public:
    std::string name() const override { return "TPer"; }
    void parse(const uint8_t* data, size_t len) override;

    bool syncSupported = false;
    bool asyncSupported = false;
    bool ackNakSupported = false;
    bool bufferMgmtSupported = false;
    bool streamingSupported = false;
    bool comIdMgmtSupported = false;
};

/// Locking Feature (0x0002)
class LockingFeature : public FeatureDescriptor {
public:
    std::string name() const override { return "Locking"; }
    void parse(const uint8_t* data, size_t len) override;

    bool lockingSupported = false;
    bool lockingEnabled = false;
    bool locked = false;
    bool mediaEncryption = false;
    bool mbrEnabled = false;
    bool mbrDone = false;
};

/// Geometry Reporting Feature (0x0003)
class GeometryFeature : public FeatureDescriptor {
public:
    std::string name() const override { return "Geometry"; }
    void parse(const uint8_t* data, size_t len) override;

    bool align = false;
    uint32_t logicalBlockSize = 512;
    uint64_t alignmentGranularity = 0;
    uint64_t lowestAlignedLBA = 0;
};

/// Opal SSC v1.0 Feature (0x0200)
class OpalV1Feature : public FeatureDescriptor {
public:
    std::string name() const override { return "Opal v1.0"; }
    void parse(const uint8_t* data, size_t len) override;

    uint16_t baseComId = 0;
    uint16_t numComIds = 0;
    bool rangeCrossing = false;
};

/// Opal SSC v2.0 Feature (0x0203)
class OpalV2Feature : public FeatureDescriptor {
public:
    std::string name() const override { return "Opal v2.0"; }
    void parse(const uint8_t* data, size_t len) override;

    uint16_t baseComId = 0;
    uint16_t numComIds = 0;
    bool rangeCrossing = false;
    uint16_t numLockingSPAdminsSupported = 0;
    uint16_t numLockingSPUsersSupported = 0;
    uint8_t  initialPinIndicator = 0;
    uint8_t  revertedPinIndicator = 0;
};

/// Enterprise SSC Feature (0x0100)
class EnterpriseFeature : public FeatureDescriptor {
public:
    std::string name() const override { return "Enterprise"; }
    void parse(const uint8_t* data, size_t len) override;

    uint16_t baseComId = 0;
    uint16_t numComIds = 0;
    bool rangeCrossing = false;
};

/// Pyrite SSC v1.0 Feature (0x0302)
class PyriteV1Feature : public FeatureDescriptor {
public:
    std::string name() const override { return "Pyrite v1.0"; }
    void parse(const uint8_t* data, size_t len) override;

    uint16_t baseComId = 0;
    uint16_t numComIds = 0;
    uint8_t  initialPinIndicator = 0;
    uint8_t  revertedPinIndicator = 0;
};

/// Pyrite SSC v2.0 Feature (0x0303)
class PyriteV2Feature : public FeatureDescriptor {
public:
    std::string name() const override { return "Pyrite v2.0"; }
    void parse(const uint8_t* data, size_t len) override;

    uint16_t baseComId = 0;
    uint16_t numComIds = 0;
    uint8_t  initialPinIndicator = 0;
    uint8_t  revertedPinIndicator = 0;
};

/// Unknown/generic feature descriptor
class UnknownFeature : public FeatureDescriptor {
public:
    std::string name() const override { return "Unknown(0x" + std::to_string(featureCode_) + ")"; }
    void parse(const uint8_t* data, size_t len) override;

    Bytes rawData;
};

/// Separate header files just re-export from here
// tper_feature.h, locking_feature.h, etc. can just #include "feature_descriptor.h"

} // namespace libsed
