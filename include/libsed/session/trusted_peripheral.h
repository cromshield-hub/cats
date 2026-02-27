#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../transport/i_transport.h"
#include "../discovery/discovery.h"
#include <memory>

namespace libsed {

/// Manages Level 0 Discovery and TPer communication setup
class TrustedPeripheral {
public:
    explicit TrustedPeripheral(std::shared_ptr<ITransport> transport);

    /// Perform Level 0 Discovery
    Result discover();

    /// Get discovery results
    const DiscoveryInfo& discoveryInfo() const { return discoveryInfo_; }
    bool isDiscovered() const { return discovered_; }

    /// Get the detected SSC type
    SscType sscType() const { return discoveryInfo_.primarySsc; }

    /// Get base ComID for the SSC
    uint16_t baseComId() const { return discoveryInfo_.baseComId; }

    /// Check feature support
    bool hasLocking() const { return discoveryInfo_.lockingPresent; }
    bool isLockingEnabled() const { return discoveryInfo_.lockingEnabled; }

    /// Get transport
    std::shared_ptr<ITransport> transport() const { return transport_; }

private:
    std::shared_ptr<ITransport> transport_;
    DiscoveryInfo discoveryInfo_;
    bool discovered_ = false;
};

} // namespace libsed
