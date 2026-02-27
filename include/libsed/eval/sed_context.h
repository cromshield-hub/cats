#pragma once

/// @file sed_context.h
/// @brief 평가 플랫폼을 위한 스레드별 TCG SED 컨텍스트.
///
/// SedContext는 Worker가 TCG 작업을 수행하는 데 필요한 모든 것을 묶습니다:
///   - Transport (DI된 libnvme 포함)
///   - EvalApi 인스턴스
///   - 활성 세션(들)
///   - 캐시된 Discovery/Properties
///
/// 소유권 모델:
///
///   ┌─ NVMeThread ─────────────────────────────────────────────┐
///   │  소유: libnvme (shared_ptr<INvmeDevice>)                 │
///   │  생성: 스레드별 SedContext (libnvme → transport 주입)      │
///   │                                                           │
///   │  ┌─ Worker A ─────────────────────────────────────────┐  │
///   │  │  수신: libnvme* (NVMeThread로부터)                  │  │
///   │  │  수신: SedContext& (NVMeThread로부터)                │  │
///   │  │                                                     │  │
///   │  │  // TCG 작업                                        │  │
///   │  │  ctx.api().discovery0(ctx.transport(), info);        │  │
///   │  │  ctx.openSession(uid::SP_LOCKING, uid::AUTH_ADMIN1, │  │
///   │  │                  cred);                              │  │
///   │  │  ctx.api().getLockingInfo(ctx.session(), 0, li, raw);│  │
///   │  │  ctx.closeSession();                                │  │
///   │  │                                                     │  │
///   │  │  // NVMe 작업 (동일 장치)                            │  │
///   │  │  ctx.nvme()->identify(1, 0, data);                  │  │
///   │  │  ctx.nvme()->getLogPage(0x02, ...);                 │  │
///   │  └─────────────────────────────────────────────────────┘  │
///   │                                                           │
///   │  ┌─ Worker B ─────────────────────────────────────────┐  │
///   │  │  동일 ctx& → 동일 transport/libnvme                 │  │
///   │  │  필요에 따라 자체 세션을 열음                         │  │
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

/// @brief 스레드별 TCG SED 컨텍스트
/// 각 NVMeThread가 하나의 SedContext를 생성합니다 (libnvme를 주입).
/// Worker들은 이 컨텍스트를 통해 모든 TCG + NVMe 작업을 수행합니다.
/// 스레드 안전성: 스레드 안전하지 않음. 스레드당 하나의 SedContext를 사용하세요.
class SedContext {
public:
    // ── Construction ────────────────────────────────

    /// @brief DI된 libnvme 장치로 컨텍스트 생성
    /// 평가 플랫폼의 기본 패턴입니다.
    /// @param nvmeDevice 이 스레드의 libnvme 인스턴스
    ///
    /// 내부적으로 NvmeTransport(nvmeDevice)를 생성합니다.
    explicit SedContext(std::shared_ptr<INvmeDevice> nvmeDevice)
        : nvmeDevice_(std::move(nvmeDevice))
        , transport_(std::make_shared<NvmeTransport>(nvmeDevice_))
    {
    }

    /// @brief 미리 구성된 Transport로 컨텍스트 생성 (테스트 또는 비-NVMe용)
    /// @param transport 미리 구성된 Transport 인스턴스
    explicit SedContext(std::shared_ptr<ITransport> transport)
        : transport_(std::move(transport))
    {
        // NvmeTransport인 경우 INvmeDevice 추출 시도
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

    /// @brief EvalApi 인스턴스 반환 (상태 비보유, 편의상 컨텍스트별 제공)
    EvalApi& api() { return api_; }

    /// @brief Transport 반환 (shared_ptr, 내부에 libnvme DI)
    std::shared_ptr<ITransport> transport() const { return transport_; }

    /// @brief 기저 NVMe 장치 반환 (NVMe DI가 아닌 경우 nullptr)
    INvmeDevice* nvme() const { return nvmeDevice_.get(); }

    /// @brief NVMe 장치의 shared_ptr 반환
    std::shared_ptr<INvmeDevice> nvmeShared() const { return nvmeDevice_; }

    // ── Initialization (call once per context) ──────

    /// @brief Discovery + Properties 교환 + ComID 캐시
    /// 세션을 열기 전에 호출하세요.
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

    /// @brief 초기화 완료 여부 확인
    bool isInitialized() const { return initialized_; }

    /// @brief 캐시된 Discovery 정보 반환
    const TcgOption& tcgOption() const { return tcgOption_; }

    /// @brief 캐시된 Properties 반환
    const PropertiesResult& properties() const { return properties_; }

    /// @brief 기본 ComID 반환
    uint16_t comId() const { return comId_; }

    // ── Session Management ──────────────────────────

    /// @brief 인증을 포함한 세션 열기
    /// @param spUid SP(Security Provider) UID
    /// @param authUid 인증 Authority UID
    /// @param credential 인증 자격 증명 (바이트 배열)
    /// @param write 쓰기 세션 여부 (기본값: true)
    /// @return 성공 또는 오류 코드
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

    /// @brief 문자열 비밀번호로 세션 열기 (자동 해시 처리)
    /// @param spUid SP(Security Provider) UID
    /// @param authUid 인증 Authority UID
    /// @param password 비밀번호 문자열 (자동으로 해시됨)
    /// @param write 쓰기 세션 여부 (기본값: true)
    /// @return 성공 또는 오류 코드
    Result openSession(uint64_t spUid, uint64_t authUid,
                       const std::string& password, bool write = true) {
        return openSession(spUid, authUid,
                           HashPassword::passwordToBytes(password), write);
    }

    /// @brief 인증 없이 읽기 전용 세션 열기 (Anybody)
    /// @param spUid SP(Security Provider) UID
    /// @return 성공 또는 오류 코드
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

    /// @brief 활성 세션 종료
    void closeSession() {
        if (session_) {
            api_.closeSession(*session_);
            session_.reset();
        }
    }

    /// @brief 활성 세션 반환 (hasSession()으로 확인 필요)
    Session& session() { return *session_; }
    const Session& session() const { return *session_; }

    /// @brief 세션 열림 여부 확인
    bool hasSession() const { return session_ != nullptr; }

    /// @brief 마지막 StartSession 결과 반환
    const StartSessionResult& lastStartSessionResult() const { return lastSsr_; }

    // ── Additional Session (for dual-SP scenarios) ──

    /// @brief 독립 세션 생성 (호출자가 수명 관리)
    /// 메인 세션이 열려있는 상태에서 이중 세션 테스트용.
    /// @return 새로 생성된 Session의 unique_ptr
    std::unique_ptr<Session> createSession() const {
        auto s = std::make_unique<Session>(transport_, comId_);
        s->setMaxComPacketSize(properties_.tperMaxComPacketSize);
        return s;
    }

    /// @brief 인증을 포함한 독립 세션 생성 및 열기
    /// @param spUid SP(Security Provider) UID
    /// @param authUid 인증 Authority UID
    /// @param credential 인증 자격 증명 (바이트 배열)
    /// @param write 쓰기 세션 여부 (기본값: true)
    /// @return 성공 시 열린 Session의 unique_ptr, 실패 시 nullptr
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

    /// @brief MSID PIN 읽기 (익명 AdminSP 세션을 열고 읽은 후 닫음)
    /// @param msid 읽어온 MSID PIN을 저장할 바이트 배열 (출력)
    /// @return 성공 또는 오류 코드
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

    /// @brief 소유권 확보: MSID를 사용하여 SID 비밀번호 설정
    /// @param newSidPassword 새로 설정할 SID 비밀번호
    /// @return 성공 또는 오류 코드
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
