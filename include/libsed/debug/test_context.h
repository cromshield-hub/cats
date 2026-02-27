#pragma once

/// @file TestContext.h
/// @brief Global debug/test layer for TCG SED evaluation platform.
///
/// TestContext is a singleton (like Logger) accessible from anywhere in the
/// library.  Every operation can query it to:
///   1. Read **config overrides** (per-session or global)
///   2. Check whether a **fault injection** is armed
///   3. Check whether a **workaround** is active
///   4. Record an **event trace** for post-run analysis
///
/// Contexts are organised by a *session signature* (string key).  A zero-
/// length key (or the constant `kGlobal`) addresses the global scope; any
/// other key addresses a per-session scope that inherits from global.
///
/// Thread safety: all public methods are internally locked via a shared
/// mutex so the layer can be interrogated from transport, codec, session,
/// and SSC layers concurrently.

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <atomic>

#include "libsed/core/types.h"
#include "libsed/core/error.h"

namespace libsed {
namespace debug {

// ════════════════════════════════════════════════════════
//  Value types
// ════════════════════════════════════════════════════════

/// A debug config value – intentionally kept simple.
using ConfigValue = std::variant<
    bool,
    int64_t,
    uint64_t,
    double,
    std::string,
    Bytes
>;

// ════════════════════════════════════════════════════════
//  Fault Injection
// ════════════════════════════════════════════════════════

/// Where in the stack the fault fires.
enum class FaultPoint : uint32_t {
    // Transport layer
    BeforeIfSend          = 0x0100,
    AfterIfSend           = 0x0101,
    BeforeIfRecv          = 0x0102,
    AfterIfRecv           = 0x0103,

    // Packet layer
    BeforePacketBuild     = 0x0200,
    AfterPacketParse      = 0x0201,

    // Codec layer
    BeforeTokenEncode     = 0x0300,
    AfterTokenDecode      = 0x0301,

    // Session layer
    BeforeStartSession    = 0x0400,
    AfterStartSession     = 0x0401,
    BeforeSendMethod      = 0x0402,
    AfterRecvMethod       = 0x0403,
    BeforeCloseSession    = 0x0404,

    // Method layer
    BeforeMethodBuild     = 0x0500,
    AfterMethodParse      = 0x0501,

    // Discovery
    BeforeDiscovery       = 0x0600,
    AfterDiscovery        = 0x0601,

    // SSC layer
    BeforeOpalOp          = 0x0700,
    BeforeEnterpriseOp    = 0x0701,
    BeforePyriteOp        = 0x0702,
};

/// What the fault does when it fires.
enum class FaultAction : uint8_t {
    ReturnError,        ///< Return a specific ErrorCode
    CorruptPayload,     ///< Flip bits / truncate the byte payload
    DelayMs,            ///< Sleep for N milliseconds, then continue
    DropPacket,         ///< Silently discard the packet
    ReplacePayload,     ///< Substitute the payload entirely
    InvokeCallback,     ///< Call a user-provided lambda
};

/// Describes a single armed fault.
struct FaultRule {
    std::string       id;                    ///< Human-readable rule name
    FaultPoint        point;                 ///< Where it triggers
    FaultAction       action;                ///< What it does
    ErrorCode         errorToReturn  = ErrorCode::InternalError;
    uint32_t          delayMs        = 0;
    Bytes             replacementPayload;
    int               corruptOffset  = 0;    ///< -1 = random
    uint8_t           corruptMask    = 0xFF; ///< XOR mask for corruption
    int               hitCountdown   = -1;   ///< <0 = infinite, 0 = spent
    int               totalHits      = 0;

    /// Optional user callback (action == InvokeCallback)
    std::function<Result(Bytes& /*payload*/)> callback;

    bool isSpent() const { return hitCountdown == 0; }
    bool isArmed() const { return hitCountdown != 0; }
};

// ════════════════════════════════════════════════════════
//  Workaround Flags
// ════════════════════════════════════════════════════════

/// Well-known workaround IDs.  Users may also register arbitrary strings.
namespace workaround {
    constexpr const char* kSkipSyncSessionCheck    = "wa.skip_sync_session_check";
    constexpr const char* kForceComIdReset          = "wa.force_comid_reset";
    constexpr const char* kRetryOnSpBusy            = "wa.retry_on_sp_busy";
    constexpr const char* kExtendTimeout            = "wa.extend_timeout";
    constexpr const char* kIgnoreEndOfSession       = "wa.ignore_end_of_session";
    constexpr const char* kRelaxTokenValidation     = "wa.relax_token_validation";
    constexpr const char* kForceProtocolId          = "wa.force_protocol_id";
    constexpr const char* kOverrideMaxComPacket     = "wa.override_max_compacket";
    constexpr const char* kBypassLockingCheck       = "wa.bypass_locking_check";
    constexpr const char* kPadSmallPayloads         = "wa.pad_small_payloads";
} // namespace workaround

// ════════════════════════════════════════════════════════
//  Event Trace
// ════════════════════════════════════════════════════════

/// A single recorded trace event.
struct TraceEvent {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint         timestamp;
    std::string       sessionKey;     ///< "" for global
    FaultPoint        point;
    std::string       tag;            ///< Human label, e.g. "IF-SEND"
    std::string       detail;         ///< Free-form detail
    Bytes             snapshot;       ///< Optional payload snapshot
    ErrorCode         result = ErrorCode::Success;
};

/// Callback invoked for each trace event (observer pattern).
using TraceObserver = std::function<void(const TraceEvent&)>;

// ════════════════════════════════════════════════════════
//  Session-scoped storage
// ════════════════════════════════════════════════════════

struct SessionContext {
    std::string                                     signature;
    std::unordered_map<std::string, ConfigValue>    config;
    std::vector<FaultRule>                          faults;
    std::unordered_map<std::string, bool>           workarounds;
    std::vector<TraceEvent>                         trace;
    std::unordered_map<std::string, uint64_t>       counters;
    std::chrono::steady_clock::time_point           createdAt;
};

// ════════════════════════════════════════════════════════
//  TestContext  –  the singleton
// ════════════════════════════════════════════════════════

class TestContext {
public:
    /// Well-known session key for global scope.
    static constexpr const char* kGlobal = "";

    // ── Singleton access ─────────────────────────────

    static TestContext& instance() {
        static TestContext ctx;
        return ctx;
    }

    /// Master enable / disable (when disabled all queries return defaults).
    void enable()  { enabled_.store(true,  std::memory_order_relaxed); }
    void disable() { enabled_.store(false, std::memory_order_relaxed); }
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }

    /// Wipe everything – configs, faults, workarounds, traces, sessions.
    void reset();

    // ── Session management ───────────────────────────

    /// Create or retrieve a session context.
    void   createSession(const std::string& key);
    void   destroySession(const std::string& key);
    bool   hasSession(const std::string& key) const;
    std::vector<std::string> sessionKeys() const;

    // ── Config overrides ─────────────────────────────
    //  Lookup order: session → global → std::nullopt

    void setConfig(const std::string& key, const std::string& sessionKey, ConfigValue value);
    void setGlobalConfig(const std::string& key, ConfigValue value);
    std::optional<ConfigValue> getConfig(const std::string& key, const std::string& sessionKey = kGlobal) const;

    // Type-safe helpers
    bool        configBool  (const std::string& key, const std::string& session = kGlobal, bool        def = false)   const;
    int64_t     configInt   (const std::string& key, const std::string& session = kGlobal, int64_t     def = 0)       const;
    uint64_t    configUint  (const std::string& key, const std::string& session = kGlobal, uint64_t    def = 0)       const;
    double      configDouble(const std::string& key, const std::string& session = kGlobal, double      def = 0.0)     const;
    std::string configStr   (const std::string& key, const std::string& session = kGlobal, const std::string& def = "") const;
    Bytes       configBytes (const std::string& key, const std::string& session = kGlobal, const Bytes& def = {})     const;

    // ── Fault injection ──────────────────────────────

    /// Arm a fault rule.  Returns the rule ID for later disarming.
    std::string armFault(FaultRule rule, const std::string& sessionKey = kGlobal);

    /// Disarm (remove) a fault by ID.
    void disarmFault(const std::string& ruleId, const std::string& sessionKey = kGlobal);

    /// Disarm all faults (optionally per-session).
    void disarmAllFaults(const std::string& sessionKey = kGlobal);

    /// Check & fire: called from instrumented code.
    /// Returns Success if no fault fires; otherwise the fault's effect.
    /// If the action mutates the payload, `payload` is modified in-place.
    Result checkFault(FaultPoint point, Bytes& payload,
                      const std::string& sessionKey = kGlobal);

    /// Convenience overload when there is no payload to mutate.
    Result checkFault(FaultPoint point,
                      const std::string& sessionKey = kGlobal);

    // ── Workarounds ──────────────────────────────────

    void activateWorkaround(const std::string& waId, const std::string& sessionKey = kGlobal);
    void deactivateWorkaround(const std::string& waId, const std::string& sessionKey = kGlobal);
    bool isWorkaroundActive(const std::string& waId, const std::string& sessionKey = kGlobal) const;

    // ── Counters ─────────────────────────────────────
    //  Atomic-like bump counters keyed by arbitrary string.

    void     bumpCounter(const std::string& name, uint64_t delta = 1, const std::string& sessionKey = kGlobal);
    uint64_t getCounter (const std::string& name, const std::string& sessionKey = kGlobal) const;
    void     resetCounter(const std::string& name, const std::string& sessionKey = kGlobal);
    std::unordered_map<std::string, uint64_t> allCounters(const std::string& sessionKey = kGlobal) const;

    // ── Trace / event recording ──────────────────────

    void recordTrace(TraceEvent event);

    /// Add an observer that receives every trace event.
    void addTraceObserver(TraceObserver obs);

    /// Get recorded trace (optionally filtered by session).
    std::vector<TraceEvent> getTrace(const std::string& sessionKey = kGlobal) const;

    /// Clear trace buffer.
    void clearTrace(const std::string& sessionKey = kGlobal);

    // ── Snapshot helpers ─────────────────────────────

    /// Quick trace helper used from macros.
    void trace(FaultPoint point, const std::string& tag,
               const std::string& detail = {},
               const Bytes& snapshot = {},
               ErrorCode result = ErrorCode::Success,
               const std::string& sessionKey = kGlobal);

private:
    TestContext() = default;

    SessionContext&       getOrCreateSession(const std::string& key);
    const SessionContext* findSession(const std::string& key) const;
    SessionContext*       findSessionMut(const std::string& key);

    /// Fire a single rule; mutate `payload` if needed.
    Result fireRule(FaultRule& rule, Bytes& payload);

    mutable std::shared_mutex                              mutex_;
    std::atomic<bool>                                      enabled_{false};
    std::unordered_map<std::string, SessionContext>         sessions_;   // "" == global
    std::vector<TraceObserver>                              observers_;
    uint32_t                                               ruleIdGen_{0};
};

// ════════════════════════════════════════════════════════
//  Convenience macros  (mirror LIBSED_LOG style)
// ════════════════════════════════════════════════════════

/// Check a fault point; return early with the error if it fires.
/// Usage:  LIBSED_CHECK_FAULT(FaultPoint::BeforeIfSend, payload);
#define LIBSED_CHECK_FAULT(point, payload)                              \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled()) {                                          \
            auto _fr = _tc.checkFault(point, payload);                  \
            if (_fr.failed()) return _fr;                               \
        }                                                               \
    } while (0)

/// Check a fault point with a session key.
#define LIBSED_CHECK_FAULT_S(point, payload, sessionKey)                \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled()) {                                          \
            auto _fr = _tc.checkFault(point, payload, sessionKey);      \
            if (_fr.failed()) return _fr;                               \
        }                                                               \
    } while (0)

/// No-payload variant (e.g. session lifecycle).
#define LIBSED_CHECK_FAULT_NP(point)                                    \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled()) {                                          \
            auto _fr = _tc.checkFault(point);                           \
            if (_fr.failed()) return _fr;                               \
        }                                                               \
    } while (0)

/// Record a trace event.
#define LIBSED_TRACE_EVENT(point, tag, detail, snapshot, result)        \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled())                                            \
            _tc.trace(point, tag, detail, snapshot, result);            \
    } while (0)

/// Check if a workaround is active (evaluates to bool).
#define LIBSED_WA_ACTIVE(waId)                                          \
    (::libsed::debug::TestContext::instance().isEnabled() &&             \
     ::libsed::debug::TestContext::instance().isWorkaroundActive(waId))

/// Bump a named counter.
#define LIBSED_BUMP(name) \
    ::libsed::debug::TestContext::instance().bumpCounter(name)

} // namespace debug
} // namespace libsed
