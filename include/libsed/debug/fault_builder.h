#pragma once

/// @file FaultBuilder.h
/// @brief Fluent builder for constructing FaultRule objects.
///
/// Example:
/// @code
///   auto& tc = TestContext::instance();
///   tc.armFault(
///     FaultBuilder("drop_3rd_send")
///       .at(FaultPoint::BeforeIfSend)
///       .drop()
///       .afterHits(2)   // skip first 2, fire on 3rd
///       .once()
///       .build()
///   );
/// @endcode

#include "test_context.h"

namespace libsed {
namespace debug {

class FaultBuilder {
public:
    explicit FaultBuilder(std::string id = {}) {
        rule_.id = std::move(id);
    }

    // ── Where ────────────────────────────────────────

    FaultBuilder& at(FaultPoint point) {
        rule_.point = point;
        return *this;
    }

    // ── Action shortcuts ─────────────────────────────

    FaultBuilder& returnError(ErrorCode code) {
        rule_.action = FaultAction::ReturnError;
        rule_.errorToReturn = code;
        return *this;
    }

    FaultBuilder& corrupt(int offset = -1, uint8_t mask = 0xFF) {
        rule_.action = FaultAction::CorruptPayload;
        rule_.corruptOffset = offset;
        rule_.corruptMask = mask;
        return *this;
    }

    FaultBuilder& delay(uint32_t ms) {
        rule_.action = FaultAction::DelayMs;
        rule_.delayMs = ms;
        return *this;
    }

    FaultBuilder& drop() {
        rule_.action = FaultAction::DropPacket;
        return *this;
    }

    FaultBuilder& replaceWith(Bytes payload) {
        rule_.action = FaultAction::ReplacePayload;
        rule_.replacementPayload = std::move(payload);
        return *this;
    }

    FaultBuilder& callback(std::function<Result(Bytes&)> cb) {
        rule_.action = FaultAction::InvokeCallback;
        rule_.callback = std::move(cb);
        return *this;
    }

    // ── Firing policy ────────────────────────────────

    /// Fire at most N times, then auto-disarm.
    FaultBuilder& times(int n) {
        rule_.hitCountdown = n;
        return *this;
    }

    /// Convenience: fire exactly once.
    FaultBuilder& once() { return times(1); }

    /// Fire indefinitely (default).
    FaultBuilder& always() {
        rule_.hitCountdown = -1;
        return *this;
    }

    // ── Build ────────────────────────────────────────

    FaultRule build() const { return rule_; }

    /// Convenience: arm directly on global context.
    std::string arm(const std::string& sessionKey = TestContext::kGlobal) {
        return TestContext::instance().armFault(build(), sessionKey);
    }

private:
    FaultRule rule_;
};

} // namespace debug
} // namespace libsed
