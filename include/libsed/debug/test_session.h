#pragma once

/// @file test_session.h
/// @brief 디버그 세션을 생성하고 스코프 종료 시 파괴하는 RAII 래퍼
///
/// 사용 예시:
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
///     // ... Opal 연산 수행, 세션 키가 필요한 곳에 ts.key() 전달 ...
///   } // 세션 파괴, 모든 규칙/설정 정리됨
/// @endcode

#include "test_context.h"
#include "fault_builder.h"
#include <string>

namespace libsed {
namespace debug {

/// @brief 이름이 지정된 디버그 세션의 RAII 래퍼
/// 생성 시 세션을 만들고, 파괴 시 모든 설정/Fault/workaround/트레이스를 정리합니다.
class TestSession {
public:
    /// @brief 이름이 지정된 테스트 세션 생성
    /// @param key 세션 식별 키
    explicit TestSession(std::string key)
        : key_(std::move(key)) {
        TestContext::instance().createSession(key_);
    }

    /// @brief 세션 파괴 (모든 설정, Fault, workaround, 트레이스 제거)
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

    /// @brief 세션 키 반환 (LIBSED_CHECK_FAULT_S 등에 전달)
    /// @return 세션 키 문자열의 const 참조
    const std::string& key() const { return key_; }

    // ── Config shortcuts ─────────────────────────────

    /// @brief 설정 값 설정 단축 메서드
    /// @param name 설정 키
    /// @param value 설정 값
    /// @return 자기 자신의 참조 (메서드 체이닝용)
    TestSession& config(const std::string& name, ConfigValue value) {
        TestContext::instance().setConfig(name, key_, std::move(value));
        return *this;
    }

    /// @brief 설정 값 설정 단축 메서드 (템플릿 버전)
    /// @tparam T ConfigValue로 변환 가능한 타입
    /// @param name 설정 키
    /// @param value 설정 값
    /// @return 자기 자신의 참조 (메서드 체이닝용)
    template<typename T>
    TestSession& config(const std::string& name, T value) {
        return config(name, ConfigValue(std::move(value)));
    }

    // ── Workaround shortcuts ─────────────────────────

    /// @brief workaround 활성화/비활성화 단축 메서드
    /// @param waId workaround 식별자
    /// @param active true이면 활성화, false이면 비활성화 (기본값: true)
    /// @return 자기 자신의 참조 (메서드 체이닝용)
    TestSession& workaround(const std::string& waId, bool active = true) {
        if (active)
            TestContext::instance().activateWorkaround(waId, key_);
        else
            TestContext::instance().deactivateWorkaround(waId, key_);
        return *this;
    }

    // ── Fault shortcuts ──────────────────────────────

    /// @brief Fault 장착 단축 메서드 (FaultRule 직접 전달)
    /// @param rule 장착할 FaultRule 객체
    /// @return 장착된 규칙의 ID
    std::string fault(FaultRule rule) {
        return TestContext::instance().armFault(std::move(rule), key_);
    }

    /// @brief Fault 장착 단축 메서드 (FaultBuilder 전달)
    /// @param builder Fault를 기술하는 FaultBuilder 객체
    /// @return 장착된 규칙의 ID
    std::string fault(FaultBuilder builder) {
        return fault(builder.build());
    }

    /// @brief 특정 Fault 해제
    /// @param ruleId 해제할 규칙 ID
    void disarmFault(const std::string& ruleId) {
        TestContext::instance().disarmFault(ruleId, key_);
    }

    /// @brief 모든 Fault 해제
    void disarmAllFaults() {
        TestContext::instance().disarmAllFaults(key_);
    }

    // ── Counter shortcuts ────────────────────────────

    /// @brief 카운터 증가 단축 메서드
    /// @param name 카운터 이름
    /// @param delta 증가량 (기본값: 1)
    void bump(const std::string& name, uint64_t delta = 1) {
        TestContext::instance().bumpCounter(name, delta, key_);
    }

    /// @brief 카운터 값 읽기
    /// @param name 카운터 이름
    /// @return 현재 카운터 값
    uint64_t counter(const std::string& name) const {
        return TestContext::instance().getCounter(name, key_);
    }

    // ── Trace shortcuts ──────────────────────────────

    /// @brief 트레이스 이벤트 목록 반환
    /// @return 현재 세션의 트레이스 이벤트 벡터
    std::vector<TraceEvent> trace() const {
        return TestContext::instance().getTrace(key_);
    }

    /// @brief 트레이스 버퍼 초기화
    void clearTrace() {
        TestContext::instance().clearTrace(key_);
    }

private:
    std::string key_;
};

} // namespace debug
} // namespace libsed
