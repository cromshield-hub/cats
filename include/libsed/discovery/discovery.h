#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../transport/i_transport.h"
#include "feature_descriptor.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace libsed {

/// Level 0 Discovery parser
class Discovery {
public:
    Discovery() = default;

    /// Perform Level 0 Discovery via transport
    Result discover(std::shared_ptr<ITransport> transport);

    /// Parse raw Level 0 Discovery response
    Result parse(const uint8_t* data, size_t len);
    Result parse(const Bytes& data) { return parse(data.data(), data.size()); }

    /// Get discovery header info
    uint32_t headerLength() const { return headerLength_; }
    uint32_t majorVersion() const { return majorVersion_; }
    uint32_t minorVersion() const { return minorVersion_; }

    /// Get all feature descriptors
    const std::vector<std::unique_ptr<FeatureDescriptor>>& features() const { return features_; }

    /// Find a feature by code
    const FeatureDescriptor* findFeature(uint16_t featureCode) const;

    /// Check for specific features
    bool hasTPerFeature() const { return findFeature(0x0001) != nullptr; }
    bool hasLockingFeature() const { return findFeature(0x0002) != nullptr; }
    bool hasGeometryFeature() const { return findFeature(0x0003) != nullptr; }
    bool hasOpalV1Feature() const { return findFeature(0x0200) != nullptr; }
    bool hasOpalV2Feature() const { return findFeature(0x0203) != nullptr; }
    bool hasEnterpriseFeature() const { return findFeature(0x0100) != nullptr; }
    bool hasPyriteV1Feature() const { return findFeature(0x0302) != nullptr; }
    bool hasPyriteV2Feature() const { return findFeature(0x0303) != nullptr; }

    /// Determine primary SSC
    SscType detectSsc() const;

    /// Get base ComID for the detected SSC
    uint16_t baseComId() const;

    /// Build a DiscoveryInfo summary
    DiscoveryInfo buildInfo() const;

    /// Protocol ID for Level 0 Discovery
    static constexpr uint8_t PROTOCOL_ID = 0x01;
    /// ComID for Level 0 Discovery
    static constexpr uint16_t COMID = 0x0001;

private:
    Result parseFeature(const uint8_t* data, size_t len, size_t& offset);

    uint32_t headerLength_ = 0;
    uint32_t majorVersion_ = 0;
    uint32_t minorVersion_ = 0;
    std::vector<std::unique_ptr<FeatureDescriptor>> features_;
};

} // namespace libsed
