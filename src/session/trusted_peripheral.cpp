#include "libsed/session/trusted_peripheral.h"
#include "libsed/core/log.h"

namespace libsed {

TrustedPeripheral::TrustedPeripheral(std::shared_ptr<ITransport> transport)
    : transport_(std::move(transport)) {}

Result TrustedPeripheral::discover() {
    Discovery disc;
    auto r = disc.discover(transport_);
    if (r.failed()) {
        LIBSED_ERROR("Level 0 Discovery failed");
        return r;
    }

    discoveryInfo_ = disc.buildInfo();
    discovered_ = true;

    LIBSED_INFO("Discovery complete: SSC=%d, ComID=0x%04X, Locking=%s",
                static_cast<int>(discoveryInfo_.primarySsc),
                discoveryInfo_.baseComId,
                discoveryInfo_.lockingPresent ? "yes" : "no");

    return ErrorCode::Success;
}

} // namespace libsed
