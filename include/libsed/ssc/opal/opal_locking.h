#pragma once

#include "../../core/types.h"
#include "../../core/error.h"
#include "../../transport/i_transport.h"
#include <memory>
#include <string>
#include <optional>

namespace libsed {

/// Opal locking range operations
class OpalLocking {
public:
    OpalLocking(std::shared_ptr<ITransport> transport, uint16_t comId);

    /// Configure a locking range
    Result configureRange(const std::string& admin1Password,
                           uint32_t rangeId,
                           uint64_t rangeStart,
                           uint64_t rangeLength,
                           bool readLockEnabled = true,
                           bool writeLockEnabled = true);

    /// Enable read/write lock on a range
    Result setLockEnabled(const std::string& admin1Password,
                           uint32_t rangeId,
                           bool readLockEnabled,
                           bool writeLockEnabled);

    /// Lock a range
    Result lock(const std::string& userPassword,
                uint32_t rangeId,
                uint32_t userId = 1,
                bool readLock = true,
                bool writeLock = true);

    /// Unlock a range
    Result unlock(const std::string& userPassword,
                  uint32_t rangeId,
                  uint32_t userId = 1);

    /// Get locking range info
    Result getRangeInfo(const std::string& password,
                        uint32_t rangeId,
                        LockingRangeInfo& info,
                        uint32_t userId = 1);

    /// Generate a new encryption key for a range (crypto erase)
    Result cryptoErase(const std::string& admin1Password,
                        uint32_t rangeId);

    /// Lock/unlock global range
    Result lockGlobal(const std::string& password, uint32_t userId = 1);
    Result unlockGlobal(const std::string& password, uint32_t userId = 1);

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
};

} // namespace libsed
