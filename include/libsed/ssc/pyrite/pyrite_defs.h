#pragma once

#include "../../core/types.h"

namespace libsed {
namespace pyrite {

inline constexpr uint16_t FEATURE_CODE_V1 = 0x0302;
inline constexpr uint16_t FEATURE_CODE_V2 = 0x0303;

/// Pyrite does NOT support encryption. Access control only.
/// No crypto erase, no media encryption keys.

} // namespace pyrite
} // namespace libsed
