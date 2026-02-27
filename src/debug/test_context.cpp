#include "libsed/debug/test_context.h"
#include "libsed/core/log.h"
#include <algorithm>
#include <random>
#include <thread>
#include <sstream>

namespace libsed {
namespace debug {

// ════════════════════════════════════════════════════════
//  Reset
// ════════════════════════════════════════════════════════

void TestContext::reset() {
    std::unique_lock lock(mutex_);
    sessions_.clear();
    observers_.clear();
    ruleIdGen_ = 0;
    LIBSED_INFO("TestContext reset");
}

// ════════════════════════════════════════════════════════
//  Session management
// ════════════════════════════════════════════════════════

void TestContext::createSession(const std::string& key) {
    std::unique_lock lock(mutex_);
    auto& s = sessions_[key];
    s.signature = key;
    s.createdAt = std::chrono::steady_clock::now();
    LIBSED_DEBUG("TestContext: session '%s' created", key.c_str());
}

void TestContext::destroySession(const std::string& key) {
    if (key.empty()) return; // never destroy global
    std::unique_lock lock(mutex_);
    sessions_.erase(key);
    LIBSED_DEBUG("TestContext: session '%s' destroyed", key.c_str());
}

bool TestContext::hasSession(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return sessions_.count(key) > 0;
}

std::vector<std::string> TestContext::sessionKeys() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(sessions_.size());
    for (const auto& [k, _] : sessions_) keys.push_back(k);
    return keys;
}

SessionContext& TestContext::getOrCreateSession(const std::string& key) {
    // Caller must hold write lock
    auto& s = sessions_[key];
    if (s.signature.empty() && !key.empty()) {
        s.signature = key;
        s.createdAt = std::chrono::steady_clock::now();
    }
    return s;
}

const SessionContext* TestContext::findSession(const std::string& key) const {
    auto it = sessions_.find(key);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

SessionContext* TestContext::findSessionMut(const std::string& key) {
    auto it = sessions_.find(key);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

// ════════════════════════════════════════════════════════
//  Config overrides
// ════════════════════════════════════════════════════════

void TestContext::setConfig(const std::string& key, const std::string& sessionKey, ConfigValue value) {
    std::unique_lock lock(mutex_);
    getOrCreateSession(sessionKey).config[key] = std::move(value);
}

void TestContext::setGlobalConfig(const std::string& key, ConfigValue value) {
    setConfig(key, kGlobal, std::move(value));
}

std::optional<ConfigValue> TestContext::getConfig(const std::string& key, const std::string& sessionKey) const {
    if (!isEnabled()) return std::nullopt;

    std::shared_lock lock(mutex_);

    // 1. Check session-specific
    if (!sessionKey.empty()) {
        if (auto* s = findSession(sessionKey)) {
            auto it = s->config.find(key);
            if (it != s->config.end()) return it->second;
        }
    }

    // 2. Fall back to global
    if (auto* g = findSession(kGlobal)) {
        auto it = g->config.find(key);
        if (it != g->config.end()) return it->second;
    }

    return std::nullopt;
}

bool TestContext::configBool(const std::string& key, const std::string& session, bool def) const {
    auto v = getConfig(key, session);
    if (!v) return def;
    if (auto* p = std::get_if<bool>(&*v)) return *p;
    if (auto* p = std::get_if<int64_t>(&*v)) return *p != 0;
    if (auto* p = std::get_if<uint64_t>(&*v)) return *p != 0;
    return def;
}

int64_t TestContext::configInt(const std::string& key, const std::string& session, int64_t def) const {
    auto v = getConfig(key, session);
    if (!v) return def;
    if (auto* p = std::get_if<int64_t>(&*v)) return *p;
    if (auto* p = std::get_if<uint64_t>(&*v)) return static_cast<int64_t>(*p);
    if (auto* p = std::get_if<bool>(&*v)) return *p ? 1 : 0;
    return def;
}

uint64_t TestContext::configUint(const std::string& key, const std::string& session, uint64_t def) const {
    auto v = getConfig(key, session);
    if (!v) return def;
    if (auto* p = std::get_if<uint64_t>(&*v)) return *p;
    if (auto* p = std::get_if<int64_t>(&*v)) return static_cast<uint64_t>(*p);
    if (auto* p = std::get_if<bool>(&*v)) return *p ? 1 : 0;
    return def;
}

double TestContext::configDouble(const std::string& key, const std::string& session, double def) const {
    auto v = getConfig(key, session);
    if (!v) return def;
    if (auto* p = std::get_if<double>(&*v)) return *p;
    if (auto* p = std::get_if<int64_t>(&*v)) return static_cast<double>(*p);
    if (auto* p = std::get_if<uint64_t>(&*v)) return static_cast<double>(*p);
    return def;
}

std::string TestContext::configStr(const std::string& key, const std::string& session, const std::string& def) const {
    auto v = getConfig(key, session);
    if (!v) return def;
    if (auto* p = std::get_if<std::string>(&*v)) return *p;
    return def;
}

Bytes TestContext::configBytes(const std::string& key, const std::string& session, const Bytes& def) const {
    auto v = getConfig(key, session);
    if (!v) return def;
    if (auto* p = std::get_if<Bytes>(&*v)) return *p;
    return def;
}

// ════════════════════════════════════════════════════════
//  Fault injection
// ════════════════════════════════════════════════════════

std::string TestContext::armFault(FaultRule rule, const std::string& sessionKey) {
    std::unique_lock lock(mutex_);

    if (rule.id.empty()) {
        std::ostringstream oss;
        oss << "fault_" << ruleIdGen_++;
        rule.id = oss.str();
    }

    auto& s = getOrCreateSession(sessionKey);
    s.faults.push_back(std::move(rule));

    LIBSED_INFO("TestContext: armed fault '%s' at 0x%04X on session '%s'",
                s.faults.back().id.c_str(),
                static_cast<uint32_t>(s.faults.back().point),
                sessionKey.c_str());

    return s.faults.back().id;
}

void TestContext::disarmFault(const std::string& ruleId, const std::string& sessionKey) {
    std::unique_lock lock(mutex_);
    auto* s = findSessionMut(sessionKey);
    if (!s) return;

    auto& faults = s->faults;
    faults.erase(
        std::remove_if(faults.begin(), faults.end(),
                        [&](const FaultRule& r) { return r.id == ruleId; }),
        faults.end());
}

void TestContext::disarmAllFaults(const std::string& sessionKey) {
    std::unique_lock lock(mutex_);
    auto* s = findSessionMut(sessionKey);
    if (s) s->faults.clear();
}

Result TestContext::checkFault(FaultPoint point, Bytes& payload,
                                const std::string& sessionKey) {
    if (!isEnabled()) return ErrorCode::Success;

    std::unique_lock lock(mutex_);

    // Check session-specific faults first, then global
    auto tryFire = [&](const std::string& key) -> std::optional<Result> {
        auto* s = findSessionMut(key);
        if (!s) return std::nullopt;

        for (auto& rule : s->faults) {
            if (rule.point != point || rule.isSpent()) continue;

            // Fire the rule
            auto result = fireRule(rule, payload);

            // Record trace
            TraceEvent ev;
            ev.timestamp   = std::chrono::steady_clock::now();
            ev.sessionKey  = key;
            ev.point       = point;
            ev.tag         = "FAULT:" + rule.id;
            ev.detail      = "action=" + std::to_string(static_cast<int>(rule.action)) +
                             " hits=" + std::to_string(rule.totalHits);
            ev.result      = result.code();
            s->trace.push_back(std::move(ev));

            // Notify observers
            if (!observers_.empty() && !s->trace.empty()) {
                const auto& last = s->trace.back();
                for (auto& obs : observers_) obs(last);
            }

            if (result.failed()) return result;
        }

        // Garbage-collect spent rules
        auto& faults = s->faults;
        faults.erase(
            std::remove_if(faults.begin(), faults.end(),
                           [](const FaultRule& r) { return r.isSpent(); }),
            faults.end());

        return std::nullopt;
    };

    // Session-scoped first
    if (!sessionKey.empty()) {
        if (auto r = tryFire(sessionKey)) {
            if (r->failed()) return *r;
        }
    }

    // Then global
    if (auto r = tryFire(kGlobal)) {
        if (r->failed()) return *r;
    }

    return ErrorCode::Success;
}

Result TestContext::checkFault(FaultPoint point, const std::string& sessionKey) {
    Bytes dummy;
    return checkFault(point, dummy, sessionKey);
}

Result TestContext::fireRule(FaultRule& rule, Bytes& payload) {
    // Decrement countdown
    if (rule.hitCountdown > 0) rule.hitCountdown--;
    rule.totalHits++;

    switch (rule.action) {
        case FaultAction::ReturnError:
            LIBSED_WARN("TestContext FAULT '%s': returning error %d",
                        rule.id.c_str(), static_cast<int>(rule.errorToReturn));
            return rule.errorToReturn;

        case FaultAction::CorruptPayload: {
            if (payload.empty()) return ErrorCode::Success;

            int offset = rule.corruptOffset;
            if (offset < 0) {
                // Random offset
                std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, payload.size() - 1);
                offset = static_cast<int>(dist(rng));
            }
            if (static_cast<size_t>(offset) < payload.size()) {
                payload[offset] ^= rule.corruptMask;
                LIBSED_WARN("TestContext FAULT '%s': corrupted byte at offset %d",
                            rule.id.c_str(), offset);
            }
            return ErrorCode::Success; // corruption doesn't block the flow
        }

        case FaultAction::DelayMs:
            LIBSED_WARN("TestContext FAULT '%s': delaying %u ms",
                        rule.id.c_str(), rule.delayMs);
            std::this_thread::sleep_for(std::chrono::milliseconds(rule.delayMs));
            return ErrorCode::Success;

        case FaultAction::DropPacket:
            LIBSED_WARN("TestContext FAULT '%s': dropping packet", rule.id.c_str());
            payload.clear();
            return ErrorCode::TransportSendFailed;

        case FaultAction::ReplacePayload:
            LIBSED_WARN("TestContext FAULT '%s': replacing payload (%zu → %zu bytes)",
                        rule.id.c_str(), payload.size(), rule.replacementPayload.size());
            payload = rule.replacementPayload;
            return ErrorCode::Success;

        case FaultAction::InvokeCallback:
            if (rule.callback) {
                LIBSED_DEBUG("TestContext FAULT '%s': invoking callback", rule.id.c_str());
                return rule.callback(payload);
            }
            return ErrorCode::Success;
    }

    return ErrorCode::Success;
}

// ════════════════════════════════════════════════════════
//  Workarounds
// ════════════════════════════════════════════════════════

void TestContext::activateWorkaround(const std::string& waId, const std::string& sessionKey) {
    std::unique_lock lock(mutex_);
    getOrCreateSession(sessionKey).workarounds[waId] = true;
    LIBSED_INFO("TestContext: workaround '%s' activated (session '%s')",
                waId.c_str(), sessionKey.c_str());
}

void TestContext::deactivateWorkaround(const std::string& waId, const std::string& sessionKey) {
    std::unique_lock lock(mutex_);
    auto* s = findSessionMut(sessionKey);
    if (s) s->workarounds.erase(waId);
}

bool TestContext::isWorkaroundActive(const std::string& waId, const std::string& sessionKey) const {
    if (!isEnabled()) return false;

    std::shared_lock lock(mutex_);

    // Session-specific first
    if (!sessionKey.empty()) {
        if (auto* s = findSession(sessionKey)) {
            auto it = s->workarounds.find(waId);
            if (it != s->workarounds.end()) return it->second;
        }
    }

    // Global fallback
    if (auto* g = findSession(kGlobal)) {
        auto it = g->workarounds.find(waId);
        if (it != g->workarounds.end()) return it->second;
    }

    return false;
}

// ════════════════════════════════════════════════════════
//  Counters
// ════════════════════════════════════════════════════════

void TestContext::bumpCounter(const std::string& name, uint64_t delta,
                               const std::string& sessionKey) {
    if (!isEnabled()) return;
    std::unique_lock lock(mutex_);
    getOrCreateSession(sessionKey).counters[name] += delta;
}

uint64_t TestContext::getCounter(const std::string& name, const std::string& sessionKey) const {
    if (!isEnabled()) return 0;
    std::shared_lock lock(mutex_);

    // Session-specific
    if (!sessionKey.empty()) {
        if (auto* s = findSession(sessionKey)) {
            auto it = s->counters.find(name);
            if (it != s->counters.end()) return it->second;
        }
    }

    // Global
    if (auto* g = findSession(kGlobal)) {
        auto it = g->counters.find(name);
        if (it != g->counters.end()) return it->second;
    }

    return 0;
}

void TestContext::resetCounter(const std::string& name, const std::string& sessionKey) {
    std::unique_lock lock(mutex_);
    auto* s = findSessionMut(sessionKey);
    if (s) s->counters.erase(name);
}

std::unordered_map<std::string, uint64_t> TestContext::allCounters(
        const std::string& sessionKey) const {
    std::shared_lock lock(mutex_);
    auto* s = findSession(sessionKey);
    return s ? s->counters : std::unordered_map<std::string, uint64_t>{};
}

// ════════════════════════════════════════════════════════
//  Trace / event recording
// ════════════════════════════════════════════════════════

void TestContext::recordTrace(TraceEvent event) {
    if (!isEnabled()) return;
    std::unique_lock lock(mutex_);

    const auto& key = event.sessionKey;
    auto& s = getOrCreateSession(key);
    s.trace.push_back(event);

    for (auto& obs : observers_) {
        obs(s.trace.back());
    }
}

void TestContext::addTraceObserver(TraceObserver obs) {
    std::unique_lock lock(mutex_);
    observers_.push_back(std::move(obs));
}

std::vector<TraceEvent> TestContext::getTrace(const std::string& sessionKey) const {
    std::shared_lock lock(mutex_);
    auto* s = findSession(sessionKey);
    return s ? s->trace : std::vector<TraceEvent>{};
}

void TestContext::clearTrace(const std::string& sessionKey) {
    std::unique_lock lock(mutex_);
    auto* s = findSessionMut(sessionKey);
    if (s) s->trace.clear();
}

void TestContext::trace(FaultPoint point, const std::string& tag,
                         const std::string& detail, const Bytes& snapshot,
                         ErrorCode result, const std::string& sessionKey) {
    TraceEvent ev;
    ev.timestamp  = std::chrono::steady_clock::now();
    ev.sessionKey = sessionKey;
    ev.point      = point;
    ev.tag        = tag;
    ev.detail     = detail;
    ev.snapshot   = snapshot;
    ev.result     = result;
    recordTrace(std::move(ev));
}

} // namespace debug
} // namespace libsed
