#pragma once

#include "types.h"

namespace libsed {
namespace uid {

// ══════════════════════════════════════════════════════
//  Session Manager UIDs
// ══════════════════════════════════════════════════════
inline constexpr uint64_t SMUID          = 0x0000000000000000FF;
inline constexpr uint64_t THIS_SP        = 0x0000000000000001;

// ══════════════════════════════════════════════════════
//  Security Provider UIDs
// ══════════════════════════════════════════════════════
inline constexpr uint64_t SP_ADMIN       = 0x0000020500000001;
inline constexpr uint64_t SP_LOCKING     = 0x0000020500000002;
inline constexpr uint64_t SP_ENTERPRISE  = 0x0000020500000003;

// ══════════════════════════════════════════════════════
//  Authority UIDs
// ══════════════════════════════════════════════════════
inline constexpr uint64_t AUTH_ANYBODY   = 0x0000000900000001;
inline constexpr uint64_t AUTH_ADMINS    = 0x0000000900000002;
inline constexpr uint64_t AUTH_MAKERS    = 0x0000000900000003;
inline constexpr uint64_t AUTH_SID       = 0x0000000900000006;
inline constexpr uint64_t AUTH_PSID      = 0x000000090001FF01;
inline constexpr uint64_t AUTH_MSID      = 0x0000000900008402;

// Admin SP authorities
inline constexpr uint64_t AUTH_ADMIN1    = 0x0000000900010001;
inline constexpr uint64_t AUTH_ADMIN2    = 0x0000000900010002;
inline constexpr uint64_t AUTH_ADMIN3    = 0x0000000900010003;
inline constexpr uint64_t AUTH_ADMIN4    = 0x0000000900010004;

// Locking SP authorities
inline constexpr uint64_t AUTH_USER1     = 0x0000000900030001;
inline constexpr uint64_t AUTH_USER2     = 0x0000000900030002;
inline constexpr uint64_t AUTH_USER3     = 0x0000000900030003;
inline constexpr uint64_t AUTH_USER4     = 0x0000000900030004;
inline constexpr uint64_t AUTH_USER5     = 0x0000000900030005;
inline constexpr uint64_t AUTH_USER6     = 0x0000000900030006;
inline constexpr uint64_t AUTH_USER7     = 0x0000000900030007;
inline constexpr uint64_t AUTH_USER8     = 0x0000000900030008;
inline constexpr uint64_t AUTH_USER9     = 0x0000000900030009;

// Enterprise authorities
inline constexpr uint64_t AUTH_ERASEMASTER    = 0x0000000900008401;
inline constexpr uint64_t AUTH_BANDMASTER0    = 0x0000000900008001;
inline constexpr uint64_t AUTH_BANDMASTER1    = 0x0000000900008002;
inline constexpr uint64_t AUTH_BANDMASTER2    = 0x0000000900008003;

// ══════════════════════════════════════════════════════
//  Table UIDs
// ══════════════════════════════════════════════════════
inline constexpr uint64_t TABLE_SP       = 0x0000020500000000;
inline constexpr uint64_t TABLE_LOCKING  = 0x0000080200000000;
inline constexpr uint64_t TABLE_MBRCTRL  = 0x0000080300000000;
inline constexpr uint64_t TABLE_MBR      = 0x0000080400000000;
inline constexpr uint64_t TABLE_ACE      = 0x0000000800000000;
inline constexpr uint64_t TABLE_AUTHORITY = 0x0000000900000000;
inline constexpr uint64_t TABLE_CPIN     = 0x0000000B00000000;
inline constexpr uint64_t TABLE_DATASTORE = 0x0000100100000000;

// Specific rows in Locking table
inline constexpr uint64_t LOCKING_GLOBALRANGE   = 0x0000080200000001;
inline constexpr uint64_t LOCKING_RANGE1        = 0x0000080200030001;
inline constexpr uint64_t LOCKING_RANGE2        = 0x0000080200030002;

// C_PIN table rows
inline constexpr uint64_t CPIN_SID       = 0x0000000B00000001;
inline constexpr uint64_t CPIN_MSID      = 0x0000000B00008402;
inline constexpr uint64_t CPIN_ADMIN1    = 0x0000000B00010001;
inline constexpr uint64_t CPIN_USER1     = 0x0000000B00030001;
inline constexpr uint64_t CPIN_USER2     = 0x0000000B00030002;

// MBR Control table rows
inline constexpr uint64_t MBRCTRL_SET    = 0x0000080300000001;

// ACE table rows (Access Control Element)
inline constexpr uint64_t ACE_LOCKING_RANGE_SET_RDLOCKED  = 0x0000000800030001;
inline constexpr uint64_t ACE_LOCKING_RANGE_SET_WRLOCKED  = 0x0000000800030002;
inline constexpr uint64_t ACE_LOCKING_GLOBALRANGE_SET_RDLOCKED = 0x0000000800000001;
inline constexpr uint64_t ACE_LOCKING_GLOBALRANGE_SET_WRLOCKED = 0x0000000800000002;

// K_AES table (encryption key)
inline constexpr uint64_t TABLE_K_AES           = 0x0000080500000000;
inline constexpr uint64_t K_AES_GLOBALRANGE     = 0x0000080500000001;

// Enterprise Band table
inline constexpr uint64_t TABLE_BAND            = 0x0000080200000000;
inline constexpr uint64_t BAND_MASTER_TABLE     = 0x0000000900008000;

// Enterprise C_PIN table rows
inline constexpr uint64_t CPIN_BANDMASTER0      = 0x0000000B00008001;
inline constexpr uint64_t CPIN_ERASEMASTER      = 0x0000000B00008401;

// DataStore table rows
inline constexpr uint64_t DATASTORE_TABLE_0     = 0x0000100100000000;

// ══════════════════════════════════════════════════════
//  Column numbers
// ══════════════════════════════════════════════════════
namespace col {
    // C_PIN table columns
    inline constexpr uint32_t PIN            = 3;
    inline constexpr uint32_t PIN_TRIES_REMAINING = 4;
    inline constexpr uint32_t PIN_CHARSETS   = 5;

    // Locking table columns
    inline constexpr uint32_t RANGE_START    = 3;
    inline constexpr uint32_t RANGE_LENGTH   = 4;
    inline constexpr uint32_t READ_LOCK_EN   = 5;
    inline constexpr uint32_t WRITE_LOCK_EN  = 6;
    inline constexpr uint32_t READ_LOCKED    = 7;
    inline constexpr uint32_t WRITE_LOCKED   = 8;
    inline constexpr uint32_t LOCK_ON_RESET  = 9;
    inline constexpr uint32_t ACTIVE_KEY     = 10;

    // MBR Control columns
    inline constexpr uint32_t MBR_ENABLE     = 1;
    inline constexpr uint32_t MBR_DONE       = 2;

    // Authority table columns
    inline constexpr uint32_t AUTH_ENABLED   = 5;
    inline constexpr uint32_t AUTH_COMMON_NAME = 1;
    inline constexpr uint32_t AUTH_IS_CLASS  = 4;

    // SP table columns
    inline constexpr uint32_t LIFECYCLE      = 6;

    // DataStore/ByteTable table columns
    inline constexpr uint32_t MAX_SIZE       = 3;
    inline constexpr uint32_t USED_SIZE      = 4;

    // K_AES columns
    inline constexpr uint32_t KEY_MODE       = 5;

    // ACE columns
    inline constexpr uint32_t ACE_BOOLEAN_EXPR = 3;
    inline constexpr uint32_t ACE_COLUMNS    = 4;
}

// ══════════════════════════════════════════════════════
//  Helper: generate authority/cpin UID from base + index
// ══════════════════════════════════════════════════════
inline Uid makeUserUid(uint32_t userIndex) {
    return Uid(0x0000000900030000ULL + userIndex);
}

inline Uid makeAdminUid(uint32_t adminIndex) {
    return Uid(0x0000000900010000ULL + adminIndex);
}

inline Uid makeBandMasterUid(uint32_t bandIndex) {
    return Uid(0x0000000900008000ULL + bandIndex);
}

inline Uid makeCpinUserUid(uint32_t userIndex) {
    return Uid(0x0000000B00030000ULL + userIndex);
}

inline Uid makeCpinAdminUid(uint32_t adminIndex) {
    return Uid(0x0000000B00010000ULL + adminIndex);
}

inline Uid makeCpinBandMasterUid(uint32_t bandIndex) {
    return Uid(0x0000000B00008000ULL + bandIndex);
}

inline Uid makeLockingRangeUid(uint32_t rangeIndex) {
    if (rangeIndex == 0) return Uid(LOCKING_GLOBALRANGE);
    return Uid(0x0000080200030000ULL + rangeIndex);
}

inline Uid makeAceLockingRangeSetRdLocked(uint32_t rangeIndex) {
    if (rangeIndex == 0) return Uid(ACE_LOCKING_GLOBALRANGE_SET_RDLOCKED);
    return Uid(0x0000000800030000ULL + rangeIndex * 2 - 1);
}

inline Uid makeAceLockingRangeSetWrLocked(uint32_t rangeIndex) {
    if (rangeIndex == 0) return Uid(ACE_LOCKING_GLOBALRANGE_SET_WRLOCKED);
    return Uid(0x0000000800030000ULL + rangeIndex * 2);
}

inline Uid makeKAesUid(uint32_t rangeIndex) {
    if (rangeIndex == 0) return Uid(K_AES_GLOBALRANGE);
    return Uid(0x0000080500030000ULL + rangeIndex);
}

} // namespace uid
} // namespace libsed
