#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../session/session.h"

namespace libsed {

/// Locking range key management helper
class RangeKey {
public:
    explicit RangeKey(Session& session) : session_(session) {}

    /// Generate a new encryption key for a locking range
    Result generateKey(uint32_t rangeId);

    /// Get the active key UID for a range
    Result getActiveKey(uint32_t rangeId, Uid& keyUid);

private:
    Session& session_;
};

} // namespace libsed
