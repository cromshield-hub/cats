#pragma once

/// @file test_context.h
/// @brief TCG SED 평가 플랫폼을 위한 전역 디버그/테스트 레이어
///
/// TestContext는 라이브러리 어디서든 접근 가능한 싱글턴(Logger와 유사)입니다.
/// 모든 연산에서 다음을 질의할 수 있습니다:
///   1. **설정 오버라이드** 읽기 (세션별 또는 전역)
///   2. **Fault 주입**이 장착되어 있는지 확인
///   3. **workaround**가 활성화되어 있는지 확인
///   4. 실행 후 분석을 위한 **이벤트 트레이스** 기록
///
/// 컨텍스트는 *세션 시그니처*(문자열 키)로 구성됩니다.
/// 길이가 0인 키(또는 상수 `kGlobal`)는 전역 스코프를 나타내며,
/// 다른 키는 전역에서 상속하는 세션별 스코프를 나타냅니다.
///
/// 스레드 안전성: 모든 public 메서드는 내부적으로 shared mutex로
/// 잠금되므로 transport, codec, session, SSC 레이어에서 동시에
/// 질의할 수 있습니다.

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

/// @brief 디버그 설정 값 — 의도적으로 단순하게 유지
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

/// @brief 스택에서 Fault가 발생하는 위치
enum class FaultPoint : uint32_t {
    // Transport 레이어
    BeforeIfSend          = 0x0100,   ///< IF-SEND 호출 전
    AfterIfSend           = 0x0101,   ///< IF-SEND 호출 후
    BeforeIfRecv          = 0x0102,   ///< IF-RECV 호출 전
    AfterIfRecv           = 0x0103,   ///< IF-RECV 호출 후

    // Packet 레이어
    BeforePacketBuild     = 0x0200,   ///< 패킷 빌드 전
    AfterPacketParse      = 0x0201,   ///< 패킷 파싱 후

    // Codec 레이어
    BeforeTokenEncode     = 0x0300,   ///< 토큰 인코딩 전
    AfterTokenDecode      = 0x0301,   ///< 토큰 디코딩 후

    // Session 레이어
    BeforeStartSession    = 0x0400,   ///< 세션 시작 전
    AfterStartSession     = 0x0401,   ///< 세션 시작 후
    BeforeSendMethod      = 0x0402,   ///< 메서드 전송 전
    AfterRecvMethod       = 0x0403,   ///< 메서드 수신 후
    BeforeCloseSession    = 0x0404,   ///< 세션 종료 전

    // Method 레이어
    BeforeMethodBuild     = 0x0500,   ///< 메서드 빌드 전
    AfterMethodParse      = 0x0501,   ///< 메서드 파싱 후

    // Discovery
    BeforeDiscovery       = 0x0600,   ///< Discovery 수행 전
    AfterDiscovery        = 0x0601,   ///< Discovery 수행 후

    // SSC 레이어
    BeforeOpalOp          = 0x0700,   ///< Opal 연산 전
    BeforeEnterpriseOp    = 0x0701,   ///< Enterprise 연산 전
    BeforePyriteOp        = 0x0702,   ///< Pyrite 연산 전
};

/// @brief Fault 발생 시 수행할 동작
enum class FaultAction : uint8_t {
    ReturnError,        ///< 특정 ErrorCode를 반환
    CorruptPayload,     ///< 바이트 페이로드의 비트를 반전/절단하여 손상
    DelayMs,            ///< N 밀리초 동안 슬립 후 계속 진행
    DropPacket,         ///< 패킷을 조용히 버림
    ReplacePayload,     ///< 페이로드를 완전히 대체
    InvokeCallback,     ///< 사용자 제공 람다를 호출
};

/// @brief 하나의 장착된 Fault를 기술하는 구조체
struct FaultRule {
    std::string       id;                    ///< 사람이 읽을 수 있는 규칙 이름
    FaultPoint        point;                 ///< Fault가 발동되는 위치
    FaultAction       action;                ///< Fault 발동 시 수행할 동작
    ErrorCode         errorToReturn  = ErrorCode::InternalError;  ///< 반환할 에러 코드 (ReturnError 시)
    uint32_t          delayMs        = 0;    ///< 지연 시간(밀리초, DelayMs 시)
    Bytes             replacementPayload;    ///< 대체 페이로드 (ReplacePayload 시)
    int               corruptOffset  = 0;    ///< 손상 오프셋 (-1 = 랜덤)
    uint8_t           corruptMask    = 0xFF; ///< 손상용 XOR 마스크
    int               hitCountdown   = -1;   ///< 남은 발동 횟수 (<0 = 무제한, 0 = 소진됨)
    int               totalHits      = 0;    ///< 총 발동 횟수

    /// @brief 선택적 사용자 콜백 (action == InvokeCallback일 때)
    std::function<Result(Bytes& /*payload*/)> callback;

    /// @brief Fault가 소진되었는지 확인
    /// @return 소진되었으면 true
    bool isSpent() const { return hitCountdown == 0; }

    /// @brief Fault가 장착 상태인지 확인
    /// @return 장착 상태이면 true
    bool isArmed() const { return hitCountdown != 0; }
};

// ════════════════════════════════════════════════════════
//  Workaround Flags
// ════════════════════════════════════════════════════════

/// @brief 잘 알려진 workaround ID 상수
/// 사용자는 임의의 문자열을 workaround ID로 등록할 수도 있습니다.
namespace workaround {
    constexpr const char* kSkipSyncSessionCheck    = "wa.skip_sync_session_check";    ///< 동기 세션 확인 건너뛰기
    constexpr const char* kForceComIdReset          = "wa.force_comid_reset";          ///< ComID 강제 리셋
    constexpr const char* kRetryOnSpBusy            = "wa.retry_on_sp_busy";           ///< SP Busy 시 재시도
    constexpr const char* kExtendTimeout            = "wa.extend_timeout";             ///< 타임아웃 연장
    constexpr const char* kIgnoreEndOfSession       = "wa.ignore_end_of_session";      ///< 세션 종료 무시
    constexpr const char* kRelaxTokenValidation     = "wa.relax_token_validation";     ///< 토큰 검증 완화
    constexpr const char* kForceProtocolId          = "wa.force_protocol_id";          ///< 프로토콜 ID 강제 지정
    constexpr const char* kOverrideMaxComPacket     = "wa.override_max_compacket";     ///< 최대 ComPacket 크기 오버라이드
    constexpr const char* kBypassLockingCheck       = "wa.bypass_locking_check";       ///< 잠금 확인 우회
    constexpr const char* kPadSmallPayloads         = "wa.pad_small_payloads";         ///< 작은 페이로드 패딩
} // namespace workaround

// ════════════════════════════════════════════════════════
//  Event Trace
// ════════════════════════════════════════════════════════

/// @brief 기록된 하나의 트레이스 이벤트
struct TraceEvent {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint         timestamp;            ///< 이벤트 발생 시각
    std::string       sessionKey;           ///< 세션 키 ("" = 전역)
    FaultPoint        point;                ///< 이벤트 발생 위치
    std::string       tag;                  ///< 사람이 읽을 수 있는 레이블 (예: "IF-SEND")
    std::string       detail;               ///< 자유 형식 상세 설명
    Bytes             snapshot;             ///< 선택적 페이로드 스냅샷
    ErrorCode         result = ErrorCode::Success;  ///< 이벤트 결과 코드
};

/// @brief 각 트레이스 이벤트에 대해 호출되는 콜백 (관찰자 패턴)
using TraceObserver = std::function<void(const TraceEvent&)>;

// ════════════════════════════════════════════════════════
//  Session-scoped storage
// ════════════════════════════════════════════════════════

/// @brief 세션 범위 저장소
/// 하나의 디버그 세션에 대한 설정, Fault, workaround, 트레이스, 카운터를 보관합니다.
struct SessionContext {
    std::string                                     signature;   ///< 세션 식별자
    std::unordered_map<std::string, ConfigValue>    config;      ///< 세션별 설정 오버라이드
    std::vector<FaultRule>                          faults;      ///< 장착된 Fault 규칙 목록
    std::unordered_map<std::string, bool>           workarounds; ///< 활성화된 workaround 맵
    std::vector<TraceEvent>                         trace;       ///< 기록된 트레이스 이벤트 목록
    std::unordered_map<std::string, uint64_t>       counters;    ///< 이름 기반 카운터
    std::chrono::steady_clock::time_point           createdAt;   ///< 세션 생성 시각
};

// ════════════════════════════════════════════════════════
//  TestContext  –  the singleton
// ════════════════════════════════════════════════════════

/// @brief 전역 디버그/테스트 컨텍스트 싱글턴
///
/// 라이브러리 전체에서 접근 가능한 싱글턴으로, 설정 오버라이드,
/// Fault 주입, workaround 플래그, 이벤트 트레이스를 관리합니다.
/// 모든 public 메서드는 내부적으로 shared mutex로 보호되어 스레드 안전합니다.
class TestContext {
public:
    /// @brief 전역 스코프를 나타내는 세션 키 상수
    static constexpr const char* kGlobal = "";

    // ── Singleton access ─────────────────────────────

    /// @brief 싱글턴 인스턴스를 반환
    /// @return TestContext 싱글턴 참조
    static TestContext& instance() {
        static TestContext ctx;
        return ctx;
    }

    /// @brief 디버그 레이어 활성화
    void enable()  { enabled_.store(true,  std::memory_order_relaxed); }

    /// @brief 디버그 레이어 비활성화 (비활성화 시 모든 질의가 기본값 반환)
    void disable() { enabled_.store(false, std::memory_order_relaxed); }

    /// @brief 디버그 레이어가 활성화되어 있는지 확인
    /// @return 활성화 상태이면 true
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }

    /// @brief 모든 상태 초기화 — 설정, Fault, workaround, 트레이스, 세션 전부 제거
    void reset();

    // ── Session management ───────────────────────────

    /// @brief 세션 컨텍스트를 생성하거나 기존 세션을 가져옴
    /// @param key 세션 식별 키
    void   createSession(const std::string& key);

    /// @brief 세션 컨텍스트를 파괴 (관련 설정, Fault, workaround, 트레이스 모두 제거)
    /// @param key 파괴할 세션 키
    void   destroySession(const std::string& key);

    /// @brief 지정된 키의 세션이 존재하는지 확인
    /// @param key 확인할 세션 키
    /// @return 세션이 존재하면 true
    bool   hasSession(const std::string& key) const;

    /// @brief 현재 등록된 모든 세션 키 목록을 반환
    /// @return 세션 키 벡터
    std::vector<std::string> sessionKeys() const;

    // ── Config overrides ─────────────────────────────
    //  Lookup order: session → global → std::nullopt

    /// @brief 특정 세션에 설정 값을 저장
    /// @param key 설정 키
    /// @param sessionKey 대상 세션 키
    /// @param value 설정 값
    void setConfig(const std::string& key, const std::string& sessionKey, ConfigValue value);

    /// @brief 전역 스코프에 설정 값을 저장
    /// @param key 설정 키
    /// @param value 설정 값
    void setGlobalConfig(const std::string& key, ConfigValue value);

    /// @brief 설정 값을 조회 (세션 → 전역 순으로 검색)
    /// @param key 설정 키
    /// @param sessionKey 세션 키 (기본값: 전역)
    /// @return 설정 값 (없으면 std::nullopt)
    std::optional<ConfigValue> getConfig(const std::string& key, const std::string& sessionKey = kGlobal) const;

    /// @brief bool 타입 설정 값 조회
    /// @param key 설정 키
    /// @param session 세션 키
    /// @param def 기본값
    /// @return 설정 값 또는 기본값
    bool        configBool  (const std::string& key, const std::string& session = kGlobal, bool        def = false)   const;

    /// @brief int64_t 타입 설정 값 조회
    /// @param key 설정 키
    /// @param session 세션 키
    /// @param def 기본값
    /// @return 설정 값 또는 기본값
    int64_t     configInt   (const std::string& key, const std::string& session = kGlobal, int64_t     def = 0)       const;

    /// @brief uint64_t 타입 설정 값 조회
    /// @param key 설정 키
    /// @param session 세션 키
    /// @param def 기본값
    /// @return 설정 값 또는 기본값
    uint64_t    configUint  (const std::string& key, const std::string& session = kGlobal, uint64_t    def = 0)       const;

    /// @brief double 타입 설정 값 조회
    /// @param key 설정 키
    /// @param session 세션 키
    /// @param def 기본값
    /// @return 설정 값 또는 기본값
    double      configDouble(const std::string& key, const std::string& session = kGlobal, double      def = 0.0)     const;

    /// @brief 문자열 타입 설정 값 조회
    /// @param key 설정 키
    /// @param session 세션 키
    /// @param def 기본값
    /// @return 설정 값 또는 기본값
    std::string configStr   (const std::string& key, const std::string& session = kGlobal, const std::string& def = "") const;

    /// @brief 바이트 배열 타입 설정 값 조회
    /// @param key 설정 키
    /// @param session 세션 키
    /// @param def 기본값
    /// @return 설정 값 또는 기본값
    Bytes       configBytes (const std::string& key, const std::string& session = kGlobal, const Bytes& def = {})     const;

    // ── Fault injection ──────────────────────────────

    /// @brief Fault 규칙을 장착
    /// @param rule 장착할 FaultRule 객체
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    /// @return 나중에 해제할 때 사용할 규칙 ID
    std::string armFault(FaultRule rule, const std::string& sessionKey = kGlobal);

    /// @brief ID로 특정 Fault를 해제
    /// @param ruleId 해제할 규칙 ID
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void disarmFault(const std::string& ruleId, const std::string& sessionKey = kGlobal);

    /// @brief 모든 Fault를 해제 (선택적으로 세션별)
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void disarmAllFaults(const std::string& sessionKey = kGlobal);

    /// @brief Fault 확인 및 발동: 계측된 코드에서 호출됨
    ///
    /// Fault가 발동되지 않으면 Success를 반환하고,
    /// 발동되면 해당 Fault의 효과를 반환합니다.
    /// 동작이 페이로드를 변경하는 경우 `payload`가 제자리에서 수정됩니다.
    /// @param point Fault 확인 위치
    /// @param payload 페이로드 (Fault에 의해 변경될 수 있음)
    /// @param sessionKey 세션 키 (기본값: 전역)
    /// @return Fault 결과 (Success이면 Fault 미발동)
    Result checkFault(FaultPoint point, Bytes& payload,
                      const std::string& sessionKey = kGlobal);

    /// @brief 페이로드가 없는 경우의 Fault 확인 편의 오버로드
    /// @param point Fault 확인 위치
    /// @param sessionKey 세션 키 (기본값: 전역)
    /// @return Fault 결과 (Success이면 Fault 미발동)
    Result checkFault(FaultPoint point,
                      const std::string& sessionKey = kGlobal);

    // ── Workarounds ──────────────────────────────────

    /// @brief workaround를 활성화
    /// @param waId workaround 식별자
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void activateWorkaround(const std::string& waId, const std::string& sessionKey = kGlobal);

    /// @brief workaround를 비활성화
    /// @param waId workaround 식별자
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void deactivateWorkaround(const std::string& waId, const std::string& sessionKey = kGlobal);

    /// @brief workaround가 활성화되어 있는지 확인
    /// @param waId workaround 식별자
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    /// @return 활성화 상태이면 true
    bool isWorkaroundActive(const std::string& waId, const std::string& sessionKey = kGlobal) const;

    // ── Counters ─────────────────────────────────────
    //  Atomic-like bump counters keyed by arbitrary string.

    /// @brief 이름 기반 카운터를 증가
    /// @param name 카운터 이름
    /// @param delta 증가량 (기본값: 1)
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void     bumpCounter(const std::string& name, uint64_t delta = 1, const std::string& sessionKey = kGlobal);

    /// @brief 카운터 값을 조회
    /// @param name 카운터 이름
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    /// @return 현재 카운터 값
    uint64_t getCounter (const std::string& name, const std::string& sessionKey = kGlobal) const;

    /// @brief 카운터를 0으로 리셋
    /// @param name 카운터 이름
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void     resetCounter(const std::string& name, const std::string& sessionKey = kGlobal);

    /// @brief 모든 카운터의 이름-값 맵을 반환
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    /// @return 카운터 이름과 값의 맵
    std::unordered_map<std::string, uint64_t> allCounters(const std::string& sessionKey = kGlobal) const;

    // ── Trace / event recording ──────────────────────

    /// @brief 트레이스 이벤트를 기록
    /// @param event 기록할 TraceEvent 객체
    void recordTrace(TraceEvent event);

    /// @brief 모든 트레이스 이벤트를 수신하는 관찰자를 추가
    /// @param obs 트레이스 이벤트 수신 콜백
    void addTraceObserver(TraceObserver obs);

    /// @brief 기록된 트레이스를 반환 (선택적으로 세션별 필터링)
    /// @param sessionKey 세션 키 (기본값: 전역 — 모든 이벤트 반환)
    /// @return 트레이스 이벤트 벡터
    std::vector<TraceEvent> getTrace(const std::string& sessionKey = kGlobal) const;

    /// @brief 트레이스 버퍼를 초기화
    /// @param sessionKey 대상 세션 키 (기본값: 전역)
    void clearTrace(const std::string& sessionKey = kGlobal);

    // ── Snapshot helpers ─────────────────────────────

    /// @brief 매크로에서 사용하는 간편 트레이스 기록 헬퍼
    /// @param point 이벤트 발생 위치
    /// @param tag 사람이 읽을 수 있는 레이블 (예: "IF-SEND")
    /// @param detail 자유 형식 상세 설명
    /// @param snapshot 선택적 페이로드 스냅샷
    /// @param result 이벤트 결과 코드
    /// @param sessionKey 세션 키 (기본값: 전역)
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

    /// @brief 단일 규칙을 발동하고, 필요시 `payload`를 변경
    /// @param rule 발동할 FaultRule
    /// @param payload 변경 대상 페이로드
    /// @return Fault 결과
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

/// @brief Fault 지점을 확인하고, Fault 발동 시 에러를 즉시 반환하는 매크로
/// @param point FaultPoint 열거값 (예: FaultPoint::BeforeIfSend)
/// @param payload 페이로드 (Bytes&)
/// 사용법: LIBSED_CHECK_FAULT(FaultPoint::BeforeIfSend, payload);
#define LIBSED_CHECK_FAULT(point, payload)                              \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled()) {                                          \
            auto _fr = _tc.checkFault(point, payload);                  \
            if (_fr.failed()) return _fr;                               \
        }                                                               \
    } while (0)

/// @brief 세션 키를 지정하여 Fault 지점을 확인하는 매크로
/// @param point FaultPoint 열거값
/// @param payload 페이로드 (Bytes&)
/// @param sessionKey 세션 키 (std::string)
#define LIBSED_CHECK_FAULT_S(point, payload, sessionKey)                \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled()) {                                          \
            auto _fr = _tc.checkFault(point, payload, sessionKey);      \
            if (_fr.failed()) return _fr;                               \
        }                                                               \
    } while (0)

/// @brief 페이로드 없이 Fault 지점을 확인하는 매크로 (예: 세션 생명주기)
/// @param point FaultPoint 열거값
#define LIBSED_CHECK_FAULT_NP(point)                                    \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled()) {                                          \
            auto _fr = _tc.checkFault(point);                           \
            if (_fr.failed()) return _fr;                               \
        }                                                               \
    } while (0)

/// @brief 트레이스 이벤트를 기록하는 매크로
/// @param point FaultPoint 열거값
/// @param tag 이벤트 레이블 (문자열)
/// @param detail 상세 설명 (문자열)
/// @param snapshot 페이로드 스냅샷 (Bytes)
/// @param result 결과 코드 (ErrorCode)
#define LIBSED_TRACE_EVENT(point, tag, detail, snapshot, result)        \
    do {                                                                \
        auto& _tc = ::libsed::debug::TestContext::instance();           \
        if (_tc.isEnabled())                                            \
            _tc.trace(point, tag, detail, snapshot, result);            \
    } while (0)

/// @brief workaround가 활성화되어 있는지 확인하는 매크로 (bool로 평가됨)
/// @param waId workaround 식별자 (문자열)
#define LIBSED_WA_ACTIVE(waId)                                          \
    (::libsed::debug::TestContext::instance().isEnabled() &&             \
     ::libsed::debug::TestContext::instance().isWorkaroundActive(waId))

/// @brief 이름 기반 카운터를 1 증가시키는 매크로
/// @param name 카운터 이름 (문자열)
#define LIBSED_BUMP(name) \
    ::libsed::debug::TestContext::instance().bumpCounter(name)

} // namespace debug
} // namespace libsed
