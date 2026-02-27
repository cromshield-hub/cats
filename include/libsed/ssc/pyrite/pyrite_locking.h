#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>

namespace libsed {

/// Pyrite locking operations (access control only, no encryption)
class PyriteLocking {
public:
    PyriteLocking(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Lock a range (logical lock, no crypto)
    Result lock(const std::string& userPassword,
                uint32_t rangeId = 0,
                uint32_t userId = 1);

    /// Unlock a range
    Result unlock(const std::string& userPassword,
                  uint32_t rangeId = 0,
                  uint32_t userId = 1);

    /// Get locking range info
    Result getRangeInfo(const std::string& password,
                        uint32_t rangeId,
                        LockingRangeInfo& info,
                        uint32_t userId = 1);

    /// Note: Crypto erase is NOT supported in Pyrite
    /// Attempting will return ErrorCode::NotImplemented

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
