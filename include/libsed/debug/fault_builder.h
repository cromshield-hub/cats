#pragma once

/// @file fault_builder.h
/// @brief FaultRule 객체를 생성하기 위한 플루언트 빌더
///
/// 사용 예시:
/// @code
///   auto& tc = TestContext::instance();
///   tc.armFault(
///     FaultBuilder("drop_3rd_send")
///       .at(FaultPoint::BeforeIfSend)
///       .drop()
///       .afterHits(2)   // 처음 2회 건너뛰고, 3번째에 발동
///       .once()
///       .build()
///   );
/// @endcode

#include "test_context.h"

namespace libsed {
namespace debug {

/// @brief FaultRule을 플루언트(fluent) 방식으로 생성하는 빌더 클래스
///
/// 메서드 체이닝을 통해 Fault 위치, 동작, 발동 정책을 설정한 뒤
/// build()로 FaultRule 객체를 생성하거나, arm()으로 직접 장착할 수 있습니다.
class FaultBuilder {
public:
    /// @brief FaultBuilder 생성자
    /// @param id Fault 규칙의 식별 이름 (선택, 빈 문자열 가능)
    explicit FaultBuilder(std::string id = {}) {
        rule_.id = std::move(id);
    }

    // ── Where ────────────────────────────────────────

    /// @brief Fault 발생 위치 설정
    /// @param point 스택에서 Fault가 발동될 위치
    /// @return 빌더 자신의 참조 (메서드 체이닝용)
    FaultBuilder& at(FaultPoint point) {
        rule_.point = point;
        return *this;
    }

    // ── Action shortcuts ─────────────────────────────

    /// @brief 특정 ErrorCode를 반환하는 Fault 설정
    /// @param code 반환할 에러 코드
    /// @return 빌더 자신의 참조
    FaultBuilder& returnError(ErrorCode code) {
        rule_.action = FaultAction::ReturnError;
        rule_.errorToReturn = code;
        return *this;
    }

    /// @brief 페이로드를 손상시키는 Fault 설정
    /// @param offset 손상 오프셋 (-1 = 랜덤)
    /// @param mask 손상용 XOR 마스크
    /// @return 빌더 자신의 참조
    FaultBuilder& corrupt(int offset = -1, uint8_t mask = 0xFF) {
        rule_.action = FaultAction::CorruptPayload;
        rule_.corruptOffset = offset;
        rule_.corruptMask = mask;
        return *this;
    }

    /// @brief 지정된 밀리초만큼 지연하는 Fault 설정
    /// @param ms 지연 시간(밀리초)
    /// @return 빌더 자신의 참조
    FaultBuilder& delay(uint32_t ms) {
        rule_.action = FaultAction::DelayMs;
        rule_.delayMs = ms;
        return *this;
    }

    /// @brief 패킷을 조용히 버리는 Fault 설정
    /// @return 빌더 자신의 참조
    FaultBuilder& drop() {
        rule_.action = FaultAction::DropPacket;
        return *this;
    }

    /// @brief 페이로드를 대체하는 Fault 설정
    /// @param payload 대체할 페이로드 데이터
    /// @return 빌더 자신의 참조
    FaultBuilder& replaceWith(Bytes payload) {
        rule_.action = FaultAction::ReplacePayload;
        rule_.replacementPayload = std::move(payload);
        return *this;
    }

    /// @brief 사용자 제공 콜백을 호출하는 Fault 설정
    /// @param cb 페이로드를 인자로 받는 콜백 함수
    /// @return 빌더 자신의 참조
    FaultBuilder& callback(std::function<Result(Bytes&)> cb) {
        rule_.action = FaultAction::InvokeCallback;
        rule_.callback = std::move(cb);
        return *this;
    }

    // ── Firing policy ────────────────────────────────

    /// @brief 최대 N회 발동 후 자동 해제
    /// @param n 최대 발동 횟수
    /// @return 빌더 자신의 참조
    FaultBuilder& times(int n) {
        rule_.hitCountdown = n;
        return *this;
    }

    /// @brief 정확히 1회만 발동
    /// @return 빌더 자신의 참조
    FaultBuilder& once() { return times(1); }

    /// @brief 무제한 발동 (기본값)
    /// @return 빌더 자신의 참조
    FaultBuilder& always() {
        rule_.hitCountdown = -1;
        return *this;
    }

    // ── Build ────────────────────────────────────────

    /// @brief FaultRule 객체 생성
    /// @return 구성된 FaultRule 객체
    FaultRule build() const { return rule_; }

    /// @brief 전역 컨텍스트에 직접 장착하는 편의 메서드
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    /// @return 장착된 규칙의 ID (나중에 해제 시 사용)
    std::string arm(const std::string& sessionKey = TestContext::kGlobal) {
        return TestContext::instance().armFault(build(), sessionKey);
    }

private:
    FaultRule rule_;
};

} // namespace debug
} // namespace libsed
