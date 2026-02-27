#pragma once

/// @file sed_context.h
/// @brief Per-thread TCG SED context for evaluation platforms.
///
/// SedContext bundles everything a Worker needs to perform TCG operations:
///   - Transport (with DI'd libnvme)
///   - EvalApi instance
///   - Active Session(s)
///   - Cached Discovery/Properties
///
/// Ownership model:
///
///   ┌─ NVMeThread ─────────────────────────────────────────────┐
///   │  owns: libnvme (shared_ptr<INvmeDevice>)                 │
///   │  creates: SedContext per-thread (injects libnvme → transport)│
///   │                                                           │
///   │  ┌─ Worker A ─────────────────────────────────────────┐  │
///   │  │  receives: libnvme* (from NVMeThread)               │  │
///   │  │  receives: SedContext& (from NVMeThread)             │  │
///   │  │                                                     │  │
///   │  │  // TCG operations                                  │  │
///   │  │  ctx.api().discovery0(ctx.transport(), info);        │  │
///   │  │  ctx.openSession(uid::SP_LOCKING, uid::AUTH_ADMIN1, │  │
///   │  │                  cred);                              │  │
///   │  │  ctx.api().getLockingInfo(ctx.session(), 0, li, raw);│  │
///   │  │  ctx.closeSession();                                │  │
///   │  │                                                     │  │
///   │  │  // NVMe operations (same device)                   │  │
///   │  │  ctx.nvme()->identify(1, 0, data);                  │  │
///   │  │  ctx.nvme()->getLogPage(0x02, ...);                 │  │
///   │  └─────────────────────────────────────────────────────┘  │
///   │                                                           │
///   │  ┌─ Worker B ─────────────────────────────────────────┐  │
///   │  │  same ctx& → same transport/libnvme                 │  │
///   │  │  but opens own sessions as needed                   │  │
///   │  └─────────────────────────────────────────────────────┘  │
///   └───────────────────────────────────────────────────────────┘

#include "eval_api.h"
#include "../transport/nvme_transport.h"
#include "../transport/i_nvme_device.h"
#include "../security/hash_password.h"
#include <memory>
#include <string>

namespace libsed {
namespace eval {

/// Per-thread TCG SED context.
///
/// Each NVMeThread creates one SedContext (injecting its libnvme).
/// Workers use this context for all TCG + NVMe operations.
///
/// Thread-safety: NOT thread-safe. One SedContext per thread.
class SedContext {
public:
    // ── Construction ────────────────────────────────

    /// Create context with DI'd libnvme device.
    /// This is the primary pattern for your evaluation platform.
    ///
    /// @param nvmeDevice  Your libnvme instance (this thread's)
    ///
    /// Internally creates: NvmeTransport(nvmeDevice)
    explicit SedContext(std::shared_ptr<INvmeDevice> nvmeDevice)
        : nvmeDevice_(std::move(nvmeDevice))
        , transport_(std::make_shared<NvmeTransport>(nvmeDevice_))
    {
    }

    /// Create context with pre-built transport (for testing or non-NVMe).
    explicit SedContext(std::shared_ptr<ITransport> transport)
        : transport_(std::move(transport))
    {
        // Try to extract INvmeDevice if it's an NvmeTransport
        nvmeDevice_ = extractNvmeDevice();
    }

    ~SedContext() {
        closeSession();
    }

    // ── No copy, move OK ────────────────────────────

    SedContext(const SedContext&) = delete;
    SedContext& operator=(const SedContext&) = delete;
    SedContext(SedContext&&) = default;
    SedContext& operator=(SedContext&&) = default;

    // ── Core Accessors ──────────────────────────────

    /// The EvalApi instance (stateless, but per-context for convenience)
    EvalApi& api() { return api_; }

    /// The transport (shared_ptr, libnvme DI'd inside)
    std::shared_ptr<ITransport> transport() const { return transport_; }

    /// The underlying NVMe device (nullptr if not NVMe DI)
    INvmeDevice* nvme() const { return nvmeDevice_.get(); }

    /// Shared pointer to NVMe device
    std::shared_ptr<INvmeDevice> nvmeShared() const { return nvmeDevice_; }

    // ── Initialization (call once per context) ──────

    /// Discover + exchange properties + cache ComID.
    /// Call this before opening sessions.
    Result initialize() {
        // Discovery
        auto r = api_.getTcgOption(transport_, tcgOption_);
        if (r.failed()) return r;
        comId_ = tcgOption_.baseComId;
        if (comId_ == 0) return ErrorCode::DiscoveryFailed;

        // Properties
        r = api_.exchangeProperties(transport_, comId_, properties_);
        if (r.failed()) return r;

        initialized_ = true;
        return ErrorCode::Success;
    }

    /// Check if initialized
    bool isInitialized() const { return initialized_; }

    /// Cached discovery info
    const TcgOption& tcgOption() const { return tcgOption_; }

    /// Cached properties
    const PropertiesResult& properties() const { return properties_; }

    /// Base ComID
    uint16_t comId() const { return comId_; }

    // ── Session Management ──────────────────────────

    /// Open a session with authentication.
    /// The session is owned by this context.
    Result openSession(uint64_t spUid, uint64_t authUid,
                       const Bytes& credential, bool write = true) {
        closeSession();  // close any existing

        session_ = std::make_unique<Session>(transport_, comId_);
        session_->setMaxComPacketSize(properties_.tperMaxComPacketSize);

        StartSessionResult ssr;
        auto r = api_.startSessionWithAuth(*session_, spUid, write,
                                            authUid, credential, ssr);
        if (r.failed()) {
            session_.reset();
            return r;
        }
        lastSsr_ = ssr;
        return ErrorCode::Success;
    }

    /// Open session with string password (hashes automatically)
    Result openSession(uint64_t spUid, uint64_t authUid,
                       const std::string& password, bool write = true) {
        return openSession(spUid, authUid,
                           HashPassword::passwordToBytes(password), write);
    }

    /// Open read-only session without authentication (Anybody)
    Result openSessionAnybody(uint64_t spUid) {
        closeSession();
        session_ = std::make_unique<Session>(transport_, comId_);
        session_->setMaxComPacketSize(properties_.tperMaxComPacketSize);
        StartSessionResult ssr;
        auto r = api_.startSession(*session_, spUid, false, ssr);
        if (r.failed()) { session_.reset(); return r; }
        lastSsr_ = ssr;
        return ErrorCode::Success;
    }

    /// Close the active session
    void closeSession() {
        if (session_) {
            api_.closeSession(*session_);
            session_.reset();
        }
    }

    /// Get the active session (throws-ish: returns ref; check hasSession())
    Session& session() { return *session_; }
    const Session& session() const { return *session_; }

    /// Check if a session is open
    bool hasSession() const { return session_ != nullptr; }

    /// Last StartSession result
    const StartSessionResult& lastStartSessionResult() const { return lastSsr_; }

    // ── Additional Session (for dual-SP scenarios) ──

    /// Create an independent session (caller manages lifetime)
    /// Use for dual-session tests where main session stays open.
    std::unique_ptr<Session> createSession() const {
        auto s = std::make_unique<Session>(transport_, comId_);
        s->setMaxComPacketSize(properties_.tperMaxComPacketSize);
        return s;
    }

    /// Open an independent session with auth (caller manages)
    std::unique_ptr<Session> createAndOpenSession(
        uint64_t spUid, uint64_t authUid,
        const Bytes& credential, bool write = true) {
        auto s = createSession();
        StartSessionResult ssr;
        auto r = api_.startSessionWithAuth(*s, spUid, write,
                                            authUid, credential, ssr);
        if (r.failed()) return nullptr;
        return s;
    }

    // ── Convenience: Common Eval Patterns ───────────

    /// Read MSID PIN (opens anonymous AdminSP session, reads, closes)
    Result readMsid(Bytes& msid) {
        auto saved = std::move(session_);

        session_ = std::make_unique<Session>(transport_, comId_);
        session_->setMaxComPacketSize(properties_.tperMaxComPacketSize);
        StartSessionResult ssr;
        auto r = api_.startSession(*session_, uid::SP_ADMIN, false, ssr);
        if (r.ok()) {
            RawResult raw;
            r = api_.getCPin(*session_, uid::CPIN_MSID, msid, raw);
            api_.closeSession(*session_);
        }
        session_.reset();

        session_ = std::move(saved);
        return r;
    }

    /// Take ownership: set SID password using MSID
    Result takeOwnership(const std::string& newSidPassword) {
        Bytes msid;
        auto r = readMsid(msid);
        if (r.failed()) return r;

        r = openSession(uid::SP_ADMIN, uid::AUTH_SID, msid, true);
        if (r.failed()) return r;

        RawResult raw;
        Bytes newPin = HashPassword::passwordToBytes(newSidPassword);
        r = api_.setCPin(session(), uid::CPIN_SID, newPin, raw);
        closeSession();
        return r;
    }

private:
    std::shared_ptr<INvmeDevice> extractNvmeDevice() {
        if (!transport_) return nullptr;
        auto* nvmeTr = dynamic_cast<NvmeTransport*>(transport_.get());
        return nvmeTr ? nvmeTr->nvmeDeviceShared() : nullptr;
    }

    // Core
    std::shared_ptr<INvmeDevice> nvmeDevice_;
    std::shared_ptr<ITransport>  transport_;
    EvalApi                      api_;

    // Cached state
    bool             initialized_ = false;
    TcgOption        tcgOption_;
    PropertiesResult properties_;
    uint16_t         comId_ = 0;

    // Active session
    std::unique_ptr<Session>  session_;
    StartSessionResult        lastSsr_;
};

} // namespace eval
} // namespace libsed
