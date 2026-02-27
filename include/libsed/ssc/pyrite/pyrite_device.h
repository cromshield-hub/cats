#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include "../../discovery/discovery.h"
#include "pyrite_defs.h"
#include "pyrite_locking.h"
#include <memory>
#include <string>

namespace libsed {

/// High-level Pyrite device facade
class PyriteDevice {
public:
    PyriteDevice(std::shared_ptr<ITransport> transport,
                 uint16_t comId,
                 const DiscoveryInfo& info);

    const DiscoveryInfo& info() const { return info_; }

    PyriteLocking& locking() { return locking_; }

    /// Lock global range
    Result lock(const std::string& userPassword, uint32_t userId = 1) {
        return locking_.lock(userPassword, 0, userId);
    }

    /// Unlock global range
    Result unlock(const std::string& userPassword, uint32_t userId = 1) {
        return locking_.unlock(userPassword, 0, userId);
    }

    /// Take ownership (set SID password)
    Result takeOwnership(const std::string& newSidPassword);

    /// Revert to factory state
    Result revert(const std::string& sidPassword);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
    DiscoveryInfo info_;

    PyriteLocking locking_;
};

} // namespace libsed
