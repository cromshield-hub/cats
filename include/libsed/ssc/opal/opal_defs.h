#pragma once

#include "../../core/types.h"
#include "../../core/uid.h"

namespace libsed {
namespace opal {

/// Opal-specific constants
inline constexpr uint16_t FEATURE_CODE_V1 = 0x0200;
inline constexpr uint16_t FEATURE_CODE_V2 = 0x0203;

/// Maximum number of locking ranges in Opal
inline constexpr uint32_t MAX_LOCKING_RANGES = 9;

/// Maximum number of users (Opal 2.0)
inline constexpr uint32_t MAX_USERS = 9;

/// Maximum number of admins
inline constexpr uint32_t MAX_ADMINS = 4;

/// Lifecycle states
enum class LifecycleState : uint8_t {
    Issued          = 0x01,
    IssuedDisabled  = 0x02,
    IssuedFrozen    = 0x03,
    IssuedDisabledFrozen = 0x04,
    IssuedFailed    = 0x05,
    Manufactured    = 0x08,
    ManufacturedInactive = 0x09,
    ManufacturedDisabled = 0x0A,
};

/// Initial PIN indicator
enum class InitialPinIndicator : uint8_t {
    MsidPin = 0x00,
    SameAsOriginalManufacture = 0x01,
};

} // namespace opal
} // namespace libsed
