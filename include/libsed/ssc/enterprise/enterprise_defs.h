#pragma once

#include "../../core/types.h"
#include "../../core/uid.h"

namespace libsed {
namespace enterprise {

inline constexpr uint16_t FEATURE_CODE = 0x0100;

/// Enterprise maximum bands
inline constexpr uint32_t MAX_BANDS = 1024;

/// Enterprise-specific table UIDs
inline constexpr uint64_t TABLE_BAND = 0x0000080200000000;

/// Band status
struct BandInfo {
    uint32_t bandId = 0;
    uint64_t rangeStart = 0;
    uint64_t rangeLength = 0;
    bool     locked = false;
    bool     lockOnReset = false;
};

/// Make Band UID from index
inline Uid makeBandUid(uint32_t bandIndex) {
    return Uid(TABLE_BAND + bandIndex + 1);
}

} // namespace enterprise
} // namespace libsed
