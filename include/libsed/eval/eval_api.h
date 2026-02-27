#pragma once

/// @file eval_api.h
/// @brief Flat, step-by-step API for TCG SED evaluation platforms.
///
/// Unlike the high-level APIs (OpalAdmin, OpalLocking, etc.) that bundle
/// multiple protocol steps into one call, EvalApi exposes every individual
/// step as a standalone function.  This enables:
///
///   - Testing each protocol step in isolation
///   - Injecting faults between steps
///   - Verifying intermediate state (e.g. after StartSession but before Auth)
///   - Sending intentionally malformed or out-of-order commands
///   - Custom test sequences not covered by the standard flows
///
/// All functions operate on an explicit Session (or Transport) that the
/// caller manages.  Nothing is opened or closed implicitly.
///
/// Usage pattern:
/// @code
///   EvalApi api;
///   auto transport = TransportFactory::createNvme("/dev/nvme0");
///
///   // Step 1: Level 0 Discovery
///   DiscoveryInfo info;
///   api.discovery0(transport, info);
///
///   // Step 2: Properties exchange
///   EvalApi::PropertiesResult props;
///   api.exchangeProperties(transport, comId, props);
///
///   // Step 3: StartSession (get raw SyncSession response)
///   Session session(transport, comId);
///   EvalApi::StartSessionResult ssr;
///   api.startSession(session, uid::SP_ADMIN, true, uid::AUTH_SID, credential, ssr);
///
///   // Step 4: Authenticate (separate from session start)
///   api.authenticate(session, uid::AUTH_ADMIN1, password);
///
///   // Step 5: Set C_PIN
///   api.setCPin(session, uid::CPIN_SID, newPin);
///
///   // ... etc, each step independently testable
/// @endcode

#include "libsed/core/types.h"
#include "libsed/core/error.h"
#include "libsed/core/uid.h"
#include "libsed/transport/i_transport.h"
#include "libsed/session/session.h"
#include "libsed/discovery/discovery.h"
#include "libsed/discovery/feature_descriptor.h"
#include "libsed/method/method_result.h"
#include "libsed/codec/token_encoder.h"
#include "libsed/packet/packet_builder.h"
#include "libsed/transport/i_nvme_device.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace libsed {
namespace eval {

/// Raw method result with full token-level access
struct RawResult {
    MethodResult   methodResult;     ///< Parsed method response
    Bytes          rawSendPayload;   ///< Exact bytes sent on wire
    Bytes          rawRecvPayload;   ///< Exact bytes received from wire
    ErrorCode      transportError = ErrorCode::Success;
    ErrorCode      protocolError  = ErrorCode::Success;
};

/// StartSession request parameters (REQ + OPT fields)
struct StartSessionParams {
    uint32_t hostSessionId          = 0;  ///< 0 = auto-generate
    uint64_t spUid                  = 0;  ///< SP to open
    bool     write                  = false;

    // StartSession_OPT
    Bytes    hostChallenge;                ///< Host challenge for auth
    uint64_t hostExchangeAuthority  = 0;   ///< Authority UID (SID, Admin1, etc.)
    Bytes    hostExchangeCert;             ///< Host certificate
    uint64_t hostSigningAuthority   = 0;   ///< Signing authority
};

/// SyncSession response details (REQ + OPT fields)
struct SyncSessionResult {
    uint32_t tperSessionNumber      = 0;
    uint32_t hostSessionNumber      = 0;
    uint64_t spUid                  = 0;
    uint16_t spSessionTimeout       = 0;

    // SyncSession_OPT
    Bytes    spChallenge;
    Bytes    spExchangeCert;
    Bytes    spSigningCert;
    uint64_t transTimeout           = 0;
    uint64_t initialCredits         = 0;
    Bytes    signedHash;

    RawResult raw;
};

/// StartSession / SyncSession combined (legacy compat)
struct StartSessionResult {
    uint32_t  hostSessionNumber  = 0;
    uint32_t  tperSessionNumber  = 0;
    uint16_t  spSessionTimeout   = 0;
    RawResult raw;
};

/// Properties exchange response details
struct PropertiesResult {
    uint32_t tperMaxComPacketSize = 0;
    uint32_t tperMaxPacketSize    = 0;
    uint32_t tperMaxIndTokenSize  = 0;
    uint32_t tperMaxAggTokenSize  = 0;
    uint32_t tperMaxMethods       = 0;
    uint32_t tperMaxSubPackets    = 0;
    RawResult raw;
};

/// Get/Set operation result with column values
struct TableResult {
    std::vector<std::pair<uint32_t, Token>> columns;
    RawResult raw;
};

/// ACE (Access Control Element) info
struct AceInfo {
    Uid       aceUid;
    Bytes     booleanExpr;
    std::vector<Uid> authorities;
};

// ════════════════════════════════════════════════════════
//  TC Library util structs
//  (maps to getTcgOption, GetClass0SecurityStatus, etc.)
// ════════════════════════════════════════════════════════

/// Maps to getTcgOption — TCG drive option/capability summary
struct TcgOption {
    SscType  sscType         = SscType::Unknown;
    uint16_t baseComId       = 0;
    uint16_t numComIds       = 0;
    bool     lockingSupported = false;
    bool     lockingEnabled  = false;
    bool     locked          = false;
    bool     mbrSupported    = false;
    bool     mbrEnabled      = false;
    bool     mbrDone         = false;
    bool     mediaEncryption = false;
    uint16_t maxLockingAdmins = 0;
    uint16_t maxLockingUsers  = 0;
    uint8_t  initialPinIndicator  = 0;
    uint8_t  revertedPinIndicator = 0;
};

/// Maps to GetClass0SecurityStatus
struct SecurityStatus {
    bool     tperPresent     = false;
    bool     lockingPresent  = false;
    bool     geometryPresent = false;
    bool     opalV1Present   = false;
    bool     opalV2Present   = false;
    bool     enterprisePresent = false;
    bool     pyriteV1Present = false;
    bool     pyriteV2Present = false;
    SscType  primarySsc      = SscType::Unknown;
};

/// Maps to GetSecurityFeatureType — per-feature detail
struct SecurityFeatureInfo {
    uint16_t featureCode     = 0;
    std::string featureName;
    uint8_t  version         = 0;
    uint16_t dataLength      = 0;
    Bytes    rawFeatureData;
    // Decoded fields (varies by feature)
    uint16_t baseComId       = 0;
    uint16_t numComIds       = 0;
    bool     rangeCrossing   = false;
    // Locking specific
    bool     lockingSupported = false;
    bool     lockingEnabled  = false;
    bool     locked          = false;
    bool     mbrEnabled      = false;
    bool     mbrDone         = false;
};

/// Maps to GetLockingInfo
struct LockingInfo {
    uint32_t rangeId          = 0;
    uint64_t rangeStart       = 0;
    uint64_t rangeLength      = 0;
    bool     readLockEnabled  = false;
    bool     writeLockEnabled = false;
    bool     readLocked       = false;
    bool     writeLocked      = false;
    uint64_t activeKey        = 0;
};

/// Maps to GetByteTableInfo — Byte Table (DataStore) properties
struct ByteTableInfo {
    uint64_t tableUid         = 0;
    uint32_t maxSize          = 0;
    uint32_t usedSize         = 0;
};

/// TcgWrite/Read/Compare result
struct DataOpResult {
    Bytes    data;
    bool     compareMatch     = false;
    RawResult raw;
};

// ════════════════════════════════════════════════════════
//  The flat evaluation API
// ════════════════════════════════════════════════════════

class EvalApi {
public:
    EvalApi() = default;

    // ── Discovery ────────────────────────────────────

    /// Level 0 Discovery (Security Protocol 0x01, ComID 0x0001)
    Result discovery0(std::shared_ptr<ITransport> transport,
                      DiscoveryInfo& info);

    /// Level 0 Discovery returning raw response bytes
    Result discovery0Raw(std::shared_ptr<ITransport> transport,
                         Bytes& rawResponse);

    /// Level 0 Discovery with custom protocol/comId (for negative testing)
    Result discovery0Custom(std::shared_ptr<ITransport> transport,
                            uint8_t protocolId, uint16_t comId,
                            Bytes& rawResponse);

    // ── Properties ───────────────────────────────────

    /// Exchange Properties with TPer (SM level, no session)
    Result exchangeProperties(std::shared_ptr<ITransport> transport,
                              uint16_t comId,
                              PropertiesResult& result);

    /// Exchange Properties with custom host values
    Result exchangePropertiesCustom(std::shared_ptr<ITransport> transport,
                                    uint16_t comId,
                                    uint32_t maxComPacketSize,
                                    uint32_t maxPacketSize,
                                    uint32_t maxIndTokenSize,
                                    PropertiesResult& result);

    // ── Session lifecycle (combined) ─────────────────

    /// StartSession + SyncSession in one call
    Result startSession(Session& session,
                        uint64_t spUid,
                        bool write,
                        StartSessionResult& result);

    /// StartSession + SyncSession with inline auth
    Result startSessionWithAuth(Session& session,
                                uint64_t spUid,
                                bool write,
                                uint64_t authorityUid,
                                const Bytes& credential,
                                StartSessionResult& result);

    /// CloseSession
    Result closeSession(Session& session);

    // ── Session lifecycle (split REQ/OPT) ────────────

    /// Build and send StartSession only (REQ + OPT)
    /// Does NOT wait for SyncSession response.
    Result sendStartSession(std::shared_ptr<ITransport> transport,
                            uint16_t comId,
                            const StartSessionParams& params,
                            Bytes& rawSentPayload);

    /// Receive and parse SyncSession response
    Result recvSyncSession(std::shared_ptr<ITransport> transport,
                           uint16_t comId,
                           SyncSessionResult& result);

    /// Send StartSession + receive SyncSession with full parameter control
    Result startSyncSession(Session& session,
                            const StartSessionParams& params,
                            SyncSessionResult& result);

    // ── Authentication ───────────────────────────────

    Result authenticate(Session& session,
                        uint64_t authorityUid,
                        const Bytes& credential,
                        RawResult& result);

    Result authenticate(Session& session,
                        uint64_t authorityUid,
                        const std::string& password,
                        RawResult& result);

    // ── Table Get/Set (generic) ──────────────────────

    Result tableGet(Session& session,
                    uint64_t objectUid,
                    uint32_t startCol, uint32_t endCol,
                    TableResult& result);

    Result tableGetAll(Session& session,
                       uint64_t objectUid,
                       TableResult& result);

    Result tableSet(Session& session,
                    uint64_t objectUid,
                    const std::vector<std::pair<uint32_t, Token>>& columns,
                    RawResult& result);

    Result tableSetUint(Session& session,
                        uint64_t objectUid,
                        uint32_t column, uint64_t value,
                        RawResult& result);

    Result tableSetBool(Session& session,
                        uint64_t objectUid,
                        uint32_t column, bool value,
                        RawResult& result);

    Result tableSetBytes(Session& session,
                         uint64_t objectUid,
                         uint32_t column, const Bytes& value,
                         RawResult& result);

    // ── C_PIN operations ─────────────────────────────

    Result getCPin(Session& session,
                   uint64_t cpinUid,
                   Bytes& pin,
                   RawResult& result);

    Result setCPin(Session& session,
                   uint64_t cpinUid,
                   const Bytes& newPin,
                   RawResult& result);

    Result setCPin(Session& session,
                   uint64_t cpinUid,
                   const std::string& newPassword,
                   RawResult& result);

    // ── MBR operations ───────────────────────────────

    Result setMbrEnable(Session& session, bool enable, RawResult& result);
    Result setMbrDone(Session& session, bool done, RawResult& result);

    Result writeMbrData(Session& session,
                        uint32_t offset, const Bytes& data,
                        RawResult& result);

    Result readMbrData(Session& session,
                       uint32_t offset, uint32_t length,
                       Bytes& data, RawResult& result);

    /// Set MBR Control table to NSID=1 (common eval shortcut)
    Result setMbrControlNsidOne(Session& session, RawResult& result);

    // ── Locking Range operations ─────────────────────

    Result setRange(Session& session,
                    uint32_t rangeId,
                    uint64_t rangeStart,
                    uint64_t rangeLength,
                    bool readLockEnabled,
                    bool writeLockEnabled,
                    RawResult& result);

    Result setRangeLock(Session& session,
                        uint32_t rangeId,
                        bool readLocked,
                        bool writeLocked,
                        RawResult& result);

    Result getRangeInfo(Session& session,
                        uint32_t rangeId,
                        LockingRangeInfo& info,
                        RawResult& result);

    // ── Authority / ACE operations ───────────────────

    Result setAuthorityEnabled(Session& session,
                               uint64_t authorityUid,
                               bool enabled,
                               RawResult& result);

    Result addAuthorityToAce(Session& session,
                             uint64_t aceUid,
                             uint64_t authorityUid,
                             RawResult& result);

    Result getAceInfo(Session& session,
                      uint64_t aceUid,
                      AceInfo& info,
                      RawResult& result);

    // ── SP lifecycle ─────────────────────────────────

    Result activate(Session& session, uint64_t spUid, RawResult& result);
    Result revertSP(Session& session, uint64_t spUid, RawResult& result);

    // ── Crypto / Key operations ──────────────────────

    Result genKey(Session& session, uint64_t objectUid, RawResult& result);

    Result getRandom(Session& session, uint32_t count,
                     Bytes& randomData, RawResult& result);

    Result erase(Session& session, uint64_t objectUid, RawResult& result);

    // ── Raw method send ──────────────────────────────

    Result sendRawMethod(Session& session,
                         const Bytes& methodTokens,
                         RawResult& result);

    Result sendRawComPacket(Session& session,
                            const Bytes& comPacketData,
                            Bytes& rawResponse);

    static Bytes buildMethodCall(uint64_t invokingUid,
                                 uint64_t methodUid,
                                 const Bytes& paramTokens = {});

    static Bytes buildComPacket(Session& session, const Bytes& tokens);

    // ══════════════════════════════════════════════════
    //  TC Library Util Functions
    // ══════════════════════════════════════════════════

    /// getTcgOption — Parse Discovery and return drive capabilities summary
    Result getTcgOption(std::shared_ptr<ITransport> transport,
                        TcgOption& option);

    /// GetClass0SecurityStatus — Feature presence flags from Discovery
    Result getSecurityStatus(std::shared_ptr<ITransport> transport,
                             SecurityStatus& status);

    /// GetSecurityFeatureType — Detailed info for a specific feature code
    Result getSecurityFeature(std::shared_ptr<ITransport> transport,
                              uint16_t featureCode,
                              SecurityFeatureInfo& info);

    /// GetSecurityFeatureType — Get all features
    Result getAllSecurityFeatures(std::shared_ptr<ITransport> transport,
                                  std::vector<SecurityFeatureInfo>& features);

    /// GetLockingInfo — Read locking range info (requires active session)
    Result getLockingInfo(Session& session,
                         uint32_t rangeId,
                         LockingInfo& info,
                         RawResult& result);

    /// GetLockingInfo — Read all locking ranges
    Result getAllLockingInfo(Session& session,
                            std::vector<LockingInfo>& ranges,
                            uint32_t maxRanges,
                            RawResult& result);

    /// GetByteTableInfo — Read DataStore table properties
    Result getByteTableInfo(Session& session,
                            ByteTableInfo& info,
                            RawResult& result);

    /// TcgWrite — Write bytes to DataStore table at offset
    Result tcgWrite(Session& session,
                    uint64_t tableUid,
                    uint32_t offset,
                    const Bytes& data,
                    RawResult& result);

    /// TcgRead — Read bytes from DataStore table at offset
    Result tcgRead(Session& session,
                   uint64_t tableUid,
                   uint32_t offset,
                   uint32_t length,
                   DataOpResult& result);

    /// TcgCompare — Write then read-back and compare
    Result tcgCompare(Session& session,
                      uint64_t tableUid,
                      uint32_t offset,
                      const Bytes& expected,
                      DataOpResult& result);

    /// TcgWrite to default DataStore
    Result tcgWriteDataStore(Session& session,
                             uint32_t offset,
                             const Bytes& data,
                             RawResult& result);

    /// TcgRead from default DataStore
    Result tcgReadDataStore(Session& session,
                            uint32_t offset,
                            uint32_t length,
                            DataOpResult& result);

    // ══════════════════════════════════════════════════
    //  Table Enumeration
    // ══════════════════════════════════════════════════

    /// Next method — enumerate rows in a table
    Result tableNext(Session& session,
                     uint64_t tableUid,
                     uint64_t startRowUid,
                     std::vector<Uid>& rows,
                     uint32_t count,
                     RawResult& result);

    /// Get single column value (convenience)
    Result tableGetColumn(Session& session,
                          uint64_t objectUid,
                          uint32_t column,
                          Token& value,
                          RawResult& result);

    // ══════════════════════════════════════════════════
    //  User / Authority Management
    // ══════════════════════════════════════════════════

    /// Enable a user authority (e.g. User1 in Locking SP)
    Result enableUser(Session& session,
                      uint32_t userId,
                      RawResult& result);

    /// Disable a user authority
    Result disableUser(Session& session,
                       uint32_t userId,
                       RawResult& result);

    /// Set user password (User1..N)
    Result setUserPassword(Session& session,
                           uint32_t userId,
                           const Bytes& newPin,
                           RawResult& result);

    Result setUserPassword(Session& session,
                           uint32_t userId,
                           const std::string& newPassword,
                           RawResult& result);

    /// Check if a user is enabled
    Result isUserEnabled(Session& session,
                         uint32_t userId,
                         bool& enabled,
                         RawResult& result);

    /// Set Admin1 password in Locking SP
    Result setAdmin1Password(Session& session,
                             const Bytes& newPin,
                             RawResult& result);

    Result setAdmin1Password(Session& session,
                             const std::string& newPassword,
                             RawResult& result);

    /// Assign user to locking range (ACE manipulation)
    Result assignUserToRange(Session& session,
                             uint32_t userId,
                             uint32_t rangeId,
                             RawResult& result);

    // ══════════════════════════════════════════════════
    //  SP Lifecycle (extended)
    // ══════════════════════════════════════════════════

    /// Get SP lifecycle state (Manufactured=0, Manufactured-Inactive=8, Manufactured-Disabled=9)
    Result getSpLifecycle(Session& session,
                          uint64_t spUid,
                          uint8_t& lifecycle,
                          RawResult& result);

    /// PSID Revert (Admin SP via PSID authority)
    Result psidRevert(Session& session, RawResult& result);

    // ══════════════════════════════════════════════════
    //  MBR Extended
    // ══════════════════════════════════════════════════

    /// Get MBR status (Enable + Done flags)
    Result getMbrStatus(Session& session,
                        bool& mbrEnabled,
                        bool& mbrDone,
                        RawResult& result);

    // ══════════════════════════════════════════════════
    //  Locking Range Extended
    // ══════════════════════════════════════════════════

    /// Set LockOnReset for a range
    Result setLockOnReset(Session& session,
                          uint32_t rangeId,
                          bool lockOnReset,
                          RawResult& result);

    /// Crypto erase a range (generate new key)
    Result cryptoErase(Session& session,
                       uint32_t rangeId,
                       RawResult& result);

    // ══════════════════════════════════════════════════
    //  Enterprise SSC Specific
    // ══════════════════════════════════════════════════

    /// Configure enterprise band (start, length, lock enable)
    Result configureBand(Session& session,
                         uint32_t bandId,
                         uint64_t bandStart,
                         uint64_t bandLength,
                         bool readLockEnabled,
                         bool writeLockEnabled,
                         RawResult& result);

    /// Lock/unlock enterprise band
    Result lockBand(Session& session, uint32_t bandId, RawResult& result);
    Result unlockBand(Session& session, uint32_t bandId, RawResult& result);

    /// Get enterprise band info
    Result getBandInfo(Session& session,
                       uint32_t bandId,
                       LockingInfo& info,
                       RawResult& result);

    /// Set BandMaster password
    Result setBandMasterPassword(Session& session,
                                 uint32_t bandId,
                                 const Bytes& newPin,
                                 RawResult& result);

    /// Set EraseMaster password
    Result setEraseMasterPassword(Session& session,
                                  const Bytes& newPin,
                                  RawResult& result);

    /// Erase enterprise band
    Result eraseBand(Session& session,
                     uint32_t bandId,
                     RawResult& result);

    // ══════════════════════════════════════════════════
    //  Raw Transport Access
    // ══════════════════════════════════════════════════

    /// Raw IF-SEND (transport level, no session)
    Result rawIfSend(std::shared_ptr<ITransport> transport,
                     uint8_t protocolId,
                     uint16_t comId,
                     const Bytes& data);

    /// Raw IF-RECV (transport level, no session)
    Result rawIfRecv(std::shared_ptr<ITransport> transport,
                     uint8_t protocolId,
                     uint16_t comId,
                     Bytes& data,
                     size_t maxSize = 65536);

    // ══════════════════════════════════════════════════
    //  Session State & Control
    // ══════════════════════════════════════════════════

    /// Session state info
    struct SessionInfo {
        bool     active          = false;
        uint32_t hostSessionNumber = 0;
        uint32_t tperSessionNumber = 0;
        uint32_t maxComPacketSize  = 0;
        uint32_t timeoutMs         = 0;
        uint32_t seqNumber         = 0;
    };

    /// Query session state
    static SessionInfo getSessionInfo(const Session& session);

    /// Set session timeout
    static void setSessionTimeout(Session& session, uint32_t ms);

    /// Set max ComPacket size
    static void setSessionMaxComPacket(Session& session, uint32_t size);

    // ══════════════════════════════════════════════════
    //  ComID Management (for Protocol Reset etc.)
    // ══════════════════════════════════════════════════

    /// Stack Reset via Security Protocol 0x02
    Result stackReset(std::shared_ptr<ITransport> transport,
                      uint16_t comId);

    /// Verify ComID is active (Security Protocol 0x02, ComID management)
    Result verifyComId(std::shared_ptr<ITransport> transport,
                       uint16_t comId,
                       bool& active);

    // ══════════════════════════════════════════════════
    //  Password / Hashing Utilities
    // ══════════════════════════════════════════════════

    /// Hash password to bytes (using PBKDF2 if configured, else raw)
    static Bytes hashPassword(const std::string& password);

    /// Hash password with explicit salt and iterations
    static Bytes hashPasswordPbkdf2(const std::string& password,
                                     const Bytes& salt,
                                     uint32_t iterations);

    /// Get C_PIN TryLimit / Tries remaining
    Result getCPinTriesRemaining(Session& session,
                                 uint64_t cpinUid,
                                 uint32_t& remaining,
                                 RawResult& result);

    // ══════════════════════════════════════════════════
    //  Table Row Management (CreateRow / DeleteRow)
    // ══════════════════════════════════════════════════

    /// CreateRow — create a new row in a table
    Result tableCreateRow(Session& session,
                          uint64_t tableUid,
                          RawResult& result);

    /// DeleteRow — delete a row from a table
    Result tableDeleteRow(Session& session,
                          uint64_t rowUid,
                          RawResult& result);

    // ══════════════════════════════════════════════════
    //  Access Control (GetACL / Assign / Remove)
    // ══════════════════════════════════════════════════

    /// ACL info for an object+method combination
    struct AclInfo {
        std::vector<Uid> aceList;
        RawResult raw;
    };

    /// GetACL — read access control for a specific invoking UID + method UID
    Result getAcl(Session& session,
                  uint64_t invokingUid,
                  uint64_t methodUid,
                  AclInfo& info);

    /// Assign — assign authority to table row (for DataStore etc.)
    Result tableAssign(Session& session,
                       uint64_t tableUid,
                       uint64_t rowUid,
                       uint64_t authorityUid,
                       RawResult& result);

    /// Remove — remove authority from table row
    Result tableRemove(Session& session,
                       uint64_t tableUid,
                       uint64_t rowUid,
                       uint64_t authorityUid,
                       RawResult& result);

    // ══════════════════════════════════════════════════
    //  Convenience Single-Type Column Reads
    // ══════════════════════════════════════════════════

    /// Get single uint column value
    Result tableGetUint(Session& session,
                        uint64_t objectUid,
                        uint32_t column,
                        uint64_t& value,
                        RawResult& result);

    /// Get single bytes column value
    Result tableGetBytes(Session& session,
                         uint64_t objectUid,
                         uint32_t column,
                         Bytes& value,
                         RawResult& result);

    /// Get single bool column value
    Result tableGetBool(Session& session,
                        uint64_t objectUid,
                        uint32_t column,
                        bool& value,
                        RawResult& result);

    // ══════════════════════════════════════════════════
    //  Multi-Column Set (ColumnValues map)
    // ══════════════════════════════════════════════════

    /// Set multiple uint columns in one call
    Result tableSetMultiUint(Session& session,
                             uint64_t objectUid,
                             const std::vector<std::pair<uint32_t, uint64_t>>& columns,
                             RawResult& result);

    // ══════════════════════════════════════════════════
    //  Revert (object-level, distinct from RevertSP)
    // ══════════════════════════════════════════════════

    /// Revert method on an object (not SP-level revert)
    Result revert(Session& session, uint64_t objectUid, RawResult& result);

    // ══════════════════════════════════════════════════
    //  Clock
    // ══════════════════════════════════════════════════

    /// GetClock — read TPer clock value
    Result getClock(Session& session, uint64_t& clockValue, RawResult& result);

    // ══════════════════════════════════════════════════
    //  Authority Verification
    // ══════════════════════════════════════════════════

    /// Verify credential is valid (StartSession + Auth → close)
    Result verifyAuthority(std::shared_ptr<ITransport> transport,
                           uint16_t comId,
                           uint64_t spUid,
                           uint64_t authorityUid,
                           const Bytes& credential);

    Result verifyAuthority(std::shared_ptr<ITransport> transport,
                           uint16_t comId,
                           uint64_t spUid,
                           uint64_t authorityUid,
                           const std::string& password);

    // ══════════════════════════════════════════════════
    //  DataStore with Table Number
    // ══════════════════════════════════════════════════

    /// Write to numbered DataStore table (table 0, 1, 2...)
    Result tcgWriteDataStoreN(Session& session,
                              uint32_t tableNumber,
                              uint32_t offset,
                              const Bytes& data,
                              RawResult& result);

    /// Read from numbered DataStore table
    Result tcgReadDataStoreN(Session& session,
                             uint32_t tableNumber,
                             uint32_t offset,
                             uint32_t length,
                             DataOpResult& result);

    // ══════════════════════════════════════════════════
    //  Enterprise Band Extended
    // ══════════════════════════════════════════════════

    /// Set LockOnReset for enterprise band
    Result setBandLockOnReset(Session& session,
                              uint32_t bandId,
                              bool lockOnReset,
                              RawResult& result);

    /// Erase all bands (Enterprise EraseMaster)
    Result eraseAllBands(Session& session,
                         uint32_t maxBands,
                         RawResult& result);

    // ══════════════════════════════════════════════════
    //  Active Key / Key Management
    // ══════════════════════════════════════════════════

    /// Get active key UID for a locking range
    Result getActiveKey(Session& session,
                        uint32_t rangeId,
                        Uid& keyUid,
                        RawResult& result);

    // ══════════════════════════════════════════════════
    //  Discovery Namespace (parsed struct)
    // ══════════════════════════════════════════════════

    /// Discovery0 returning full parsed DiscoveryInfo struct
    Result discovery0Parsed(std::shared_ptr<ITransport> transport,
                            DiscoveryInfo& info,
                            RawResult& result);

    // ══════════════════════════════════════════════════
    //  NVMe Device Access (via DI transport)
    // ══════════════════════════════════════════════════

    /// Extract INvmeDevice from transport (returns nullptr if not NVMe DI)
    static INvmeDevice* getNvmeDevice(std::shared_ptr<ITransport> transport);

    /// NVMe Identify Controller (convenience — goes through INvmeDevice)
    static Result nvmeIdentify(std::shared_ptr<ITransport> transport,
                               uint8_t cns, uint32_t nsid, Bytes& data);

    /// NVMe Get Log Page
    static Result nvmeGetLogPage(std::shared_ptr<ITransport> transport,
                                 uint8_t logId, uint32_t nsid,
                                 Bytes& data, uint32_t dataLen);

    /// NVMe Get Feature
    static Result nvmeGetFeature(std::shared_ptr<ITransport> transport,
                                 uint8_t featureId, uint32_t nsid,
                                 uint32_t& cdw0, Bytes& data);

    /// NVMe Set Feature
    static Result nvmeSetFeature(std::shared_ptr<ITransport> transport,
                                 uint8_t featureId, uint32_t nsid,
                                 uint32_t cdw11, const Bytes& data = {});

    /// NVMe Format NVM
    static Result nvmeFormat(std::shared_ptr<ITransport> transport,
                             uint32_t nsid, uint8_t lbaf,
                             uint8_t ses = 0, uint8_t pi = 0);

    /// Submit arbitrary NVMe admin command
    static Result nvmeAdminCmd(std::shared_ptr<ITransport> transport,
                               NvmeAdminCmd& cmd, NvmeCompletion& cpl);

    /// Submit arbitrary NVMe IO command
    static Result nvmeIoCmd(std::shared_ptr<ITransport> transport,
                            NvmeIoCmd& cmd, NvmeCompletion& cpl);
};

// ════════════════════════════════════════════════════════
//  Convenience: common eval test sequences
// ════════════════════════════════════════════════════════

namespace sequence {

    /// Full ownership: Discovery → Properties → StartSession(Admin,SID,MSID)
    ///                → Set C_PIN(SID) → CloseSession
    /// Returns after each step, calling the observer with intermediate results.
    using StepObserver = std::function<bool(const std::string& stepName,
                                            const RawResult& result)>;

    Result takeOwnershipStepByStep(
        std::shared_ptr<ITransport> transport,
        uint16_t comId,
        const std::string& newSidPassword,
        StepObserver observer = nullptr);

    /// Full Opal setup: ownership → activate → set Admin1 → enable User1
    ///                → configure range → enable global locking
    Result fullOpalSetupStepByStep(
        std::shared_ptr<ITransport> transport,
        uint16_t comId,
        const std::string& sidPassword,
        const std::string& admin1Password,
        const std::string& user1Password,
        StepObserver observer = nullptr);

} // namespace sequence

} // namespace eval
} // namespace libsed
