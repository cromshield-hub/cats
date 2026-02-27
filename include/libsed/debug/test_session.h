#pragma once

/// @file TestSession.h
/// @brief RAII wrapper that creates a named debug session, configures it,
///        and destroys it on scope exit.
///
/// Example:
/// @code
///   {
///     TestSession ts("opal_lock_range3");
///     ts.config("timeout_override", uint64_t{60000});
///     ts.workaround(workaround::kRetryOnSpBusy);
///     ts.fault(FaultBuilder("corrupt_recv")
///                .at(FaultPoint::AfterIfRecv)
///                .corrupt()
///                .once());
///
///     // ... run Opal operations, passing ts.key() where session key needed ...
///   } // session destroyed, all rules/config cleaned up
/// @endcode

#include "test_context.h"
#include "fault_builder.h"
#include <string>

namespace libsed {
namespace debug {

class TestSession {
public:
    /// Create a named test session.
    explicit TestSession(std::string key)
        : key_(std::move(key)) {
        TestContext::instance().createSession(key_);
    }

    /// Destroy the session (removes all config, faults, workarounds, traces).
    ~TestSession() {
        TestContext::instance().destroySession(key_);
    }

    // Non-copyable, moveable
    TestSession(const TestSession&) = delete;
    TestSession& operator=(const TestSession&) = delete;
    TestSession(TestSession&& o) noexcept : key_(std::move(o.key_)) { o.key_.clear(); }
    TestSession& operator=(TestSession&& o) noexcept {
        if (this != &o) {
            if (!key_.empty()) TestContext::instance().destroySession(key_);
            key_ = std::move(o.key_);
            o.key_.clear();
        }
        return *this;
    }

    /// Session key (pass this to LIBSED_CHECK_FAULT_S etc.)
    const std::string& key() const { return key_; }

    // ── Config shortcuts ─────────────────────────────

    TestSession& config(const std::string& name, ConfigValue value) {
        TestContext::instance().setConfig(name, key_, std::move(value));
        return *this;
    }

    template<typename T>
    TestSession& config(const std::string& name, T value) {
        return config(name, ConfigValue(std::move(value)));
    }

    // ── Workaround shortcuts ─────────────────────────

    TestSession& workaround(const std::string& waId, bool active = true) {
        if (active)
            TestContext::instance().activateWorkaround(waId, key_);
        else
            TestContext::instance().deactivateWorkaround(waId, key_);
        return *this;
    }

    // ── Fault shortcuts ──────────────────────────────

    std::string fault(FaultRule rule) {
        return TestContext::instance().armFault(std::move(rule), key_);
    }

    std::string fault(FaultBuilder builder) {
        return fault(builder.build());
    }

    void disarmFault(const std::string& ruleId) {
        TestContext::instance().disarmFault(ruleId, key_);
    }

    void disarmAllFaults() {
        TestContext::instance().disarmAllFaults(key_);
    }

    // ── Counter shortcuts ────────────────────────────

    void bump(const std::string& name, uint64_t delta = 1) {
        TestContext::instance().bumpCounter(name, delta, key_);
    }

    uint64_t counter(const std::string& name) const {
        return TestContext::instance().getCounter(name, key_);
    }

    // ── Trace shortcuts ──────────────────────────────

    std::vector<TraceEvent> trace() const {
        return TestContext::instance().getTrace(key_);
    }

    void clearTrace() {
        TestContext::instance().clearTrace(key_);
    }

private:
    std::string key_;
};

} // namespace debug
} // namespace libsed
