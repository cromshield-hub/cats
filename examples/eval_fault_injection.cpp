/// @file eval_fault_injection.cpp
/// @brief Example: Using the Debug/Test Layer for NVMe SED evaluation.
///
/// This example demonstrates how an evaluation platform engineer would
/// use TestContext, FaultBuilder, TestSession, workarounds, and trace
/// to test edge cases on an NVMe TCG SED drive.

#include <libsed/sed_library.h>
#include <libsed/debug/debug.h>
#include <iostream>
#include <cassert>

using namespace libsed;
using namespace libsed::debug;

/// @scenario SP_BUSY 응답 시 재시도 검증
/// @precondition NVMe 디바이스가 열려 있고 TestContext가 활성화 가능해야 함
/// @steps
///   1. TestContext 활성화 및 TestSession 생성
///   2. FaultBuilder로 AfterRecvMethod 시점에 MethodSpBusy 에러를 3회 반환하도록 설정
///   3. workaround::kRetryOnSpBusy 활성화 — 라이브러리가 SP_BUSY 시 자동 재시도
///   4. SedDevice::open() 후 takeOwnership() 호출
///   5. 트레이스 로그 및 transport.send/recv 카운터 확인
/// @expected
///   - 처음 3회 SP_BUSY 응답 후 workaround로 재시도하여 최종 성공
///   - transport.send/recv 카운터가 재시도 횟수만큼 증가
///   - 트레이스에 SP_BUSY 에러 및 재시도 기록 남음
void scenario_sp_busy_retry(const std::string& device) {
    std::cout << "\n=== Scenario: SP_BUSY retry ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    // Create a scoped test session — everything cleans up on return
    TestSession ts("sp_busy_retry");

    // Arm: the first 3 sends will return SP_BUSY error
    ts.fault(
        FaultBuilder("busy_3x")
            .at(FaultPoint::AfterRecvMethod)
            .returnError(ErrorCode::MethodSpBusy)
            .times(3)
    );

    // Activate the retry workaround so the library should retry
    ts.workaround(workaround::kRetryOnSpBusy);

    // Run the actual operation
    auto sed = SedDevice::open(device);
    if (!sed) {
        std::cerr << "  [skip] Cannot open device\n";
        return;
    }

    // The library should internally retry and eventually succeed
    // (after the 3 injected failures are consumed)
    auto r = sed->takeOwnership("test_password");

    // Check counters
    std::cout << "  transport.send = " << ts.counter("transport.send") << "\n";
    std::cout << "  transport.recv = " << ts.counter("transport.recv") << "\n";

    // Print trace
    for (const auto& ev : ts.trace()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            ev.timestamp.time_since_epoch()).count();
        std::cout << "  [" << ms << "ms] " << ev.tag << " : " << ev.detail
                  << " (rc=" << static_cast<int>(ev.result) << ")\n";
    }
}

/// @scenario 응답 손상 후 에러 처리 검증
/// @precondition NVMe 디바이스가 열려 있고 TestContext가 활성화 가능해야 함
/// @steps
///   1. TestContext 활성화 및 TestSession 생성
///   2. FaultBuilder로 AfterIfRecv 시점에 첫 수신 패킷의 바이트 0을 0xFF로 손상 (1회)
///   3. SedDevice::open() 후 takeOwnership() 호출
///   4. 결과가 실패인지 검증 (assert)
/// @expected
///   - 첫 수신 패킷 바이트 0이 손상되어 ComPacket 파싱 실패 에러 발생
///   - takeOwnership 결과가 failed() == true
///   - 에러 메시지에 손상으로 인한 파싱 실패 내용 포함
void scenario_corrupt_response(const std::string& device) {
    std::cout << "\n=== Scenario: Corrupt response ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("corrupt_recv");

    // Corrupt byte 0 of the first received packet — this should cause
    // a ComPacket parse failure somewhere downstream
    ts.fault(
        FaultBuilder("corrupt_hdr")
            .at(FaultPoint::AfterIfRecv)
            .corrupt(0, 0xFF)   // XOR first byte
            .once()
    );

    auto sed = SedDevice::open(device);
    if (!sed) {
        std::cerr << "  [skip] Cannot open device\n";
        return;
    }

    auto r = sed->takeOwnership("test_password");
    std::cout << "  Result: " << r.message()
              << " (expected failure due to corruption)\n";
    assert(r.failed());
}

/// @scenario 가짜 Discovery 응답 주입
/// @precondition TestContext가 활성화 가능해야 함 (디바이스 불필요)
/// @steps
///   1. TestContext 활성화 및 TestSession 생성
///   2. 256바이트 가짜 Discovery 응답 바이너리 구성 (Pyrite 2.0 가장)
///   3. FaultBuilder로 AfterDiscovery 시점에 가짜 응답으로 교체 (1회)
/// @expected
///   - 주입된 가짜 Pyrite 2.0 Discovery 응답이 정상 파싱됨
///   - 실제 드라이브 응답 대신 주입된 데이터가 사용됨
///   - Discovery 파서가 조작된 길이/Feature 데이터를 올바르게 처리
void scenario_fake_discovery() {
    std::cout << "\n=== Scenario: Fake discovery payload ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("fake_discovery");

    // Build a fake Discovery response that claims Pyrite 2.0
    Bytes fakeDiscovery(256, 0);
    // (In a real eval you'd craft this byte-for-byte per the spec)
    fakeDiscovery[0] = 0x00; fakeDiscovery[1] = 0x00;
    fakeDiscovery[2] = 0x00; fakeDiscovery[3] = 100; // length

    ts.fault(
        FaultBuilder("fake_disc_resp")
            .at(FaultPoint::AfterDiscovery)
            .replaceWith(fakeDiscovery)
            .once()
    );

    std::cout << "  Fault armed: will inject fake Pyrite 2.0 discovery\n";
}

/// @scenario 느린 드라이브용 타임아웃 확장
/// @precondition TestContext가 활성화 가능해야 함 (디바이스 불필요)
/// @steps
///   1. TestContext 활성화 및 TestSession 생성
///   2. config("timeout_extend_ms", 120000)으로 타임아웃 2분 설정
///   3. workaround::kExtendTimeout 활성화
///   4. 설정된 타임아웃 값 확인
/// @expected
///   - 타임아웃 확장 workaround가 적용됨
///   - configUint("timeout_extend_ms")로 설정값(120000ms) 확인 가능
///   - 느린 Enterprise 드라이브에서 타임아웃 발생 방지
void scenario_timeout_workaround() {
    std::cout << "\n=== Scenario: Timeout extension workaround ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("slow_enterprise");

    // Set a longer timeout for this session
    ts.config("timeout_extend_ms", uint64_t{120000}); // 2 minutes
    ts.workaround(workaround::kExtendTimeout);

    std::cout << "  Timeout extended to "
              << tc.configUint("timeout_extend_ms", "slow_enterprise") << " ms\n";
}

/// @scenario 커스텀 콜백 주입으로 프로토콜 레벨 테스트
/// @precondition TestContext가 활성화 가능해야 함 (디바이스 불필요)
/// @steps
///   1. TestContext 활성화 및 TestSession 생성
///   2. FaultBuilder로 BeforeIfSend 시점에 커스텀 콜백 등록 (.always())
///   3. 콜백 내부에서 호출 횟수 카운트 및 페이로드 크기 로깅
///   4. 콜백은 ErrorCode::Success 반환 (전송 차단하지 않음)
/// @expected
///   - 매 IF-SEND마다 콜백이 호출됨
///   - 콜백에서 페이로드 크기가 로깅됨
///   - 콜백이 payload를 수정하여 프로토콜 레벨 테스트 가능 (본 예제에서는 수정하지 않음)
void scenario_custom_callback() {
    std::cout << "\n=== Scenario: Custom callback injection ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("custom_inject");

    int callCount = 0;
    ts.fault(
        FaultBuilder("log_every_send")
            .at(FaultPoint::BeforeIfSend)
            .callback([&](Bytes& payload) -> Result {
                callCount++;
                std::cout << "  [callback] Send #" << callCount
                          << ", payload size = " << payload.size() << "\n";
                // Could modify payload here for protocol-level testing
                return ErrorCode::Success; // don't block
            })
            .always()
    );

    std::cout << "  Callback fault armed (will log every IF-SEND)\n";
}

/// @scenario 전역 설정 및 세션별 오버라이드
/// @precondition TestContext가 활성화 가능해야 함 (디바이스 불필요)
/// @steps
///   1. TestContext 리셋 및 활성화
///   2. 전역 설정: max_retries=5, skip_revert_confirm=true, inject_serial="FAKE_SN_12345"
///   3. 전역 설정 값 검증 (assert)
///   4. "enterprise_test" 세션에 max_retries=10 오버라이드 설정
///   5. 오버라이드된 세션("enterprise_test")과 일반 세션("opal_test")에서 max_retries 값 비교
///   6. TestContext 비활성화
/// @expected
///   - 전역 설정(max_retries=5, skip_revert_confirm=true, inject_serial) 정상 적용
///   - "enterprise_test" 세션에서 max_retries=10 오버라이드 동작 확인
///   - "opal_test" 세션에서는 전역 값(5)으로 폴백 확인
void scenario_global_config() {
    std::cout << "\n=== Scenario: Global config ===\n";

    auto& tc = TestContext::instance();
    tc.reset();
    tc.enable();

    // Set global test parameters
    tc.setGlobalConfig("max_retries", int64_t{5});
    tc.setGlobalConfig("skip_revert_confirm", true);
    tc.setGlobalConfig("inject_serial", std::string{"FAKE_SN_12345"});

    // Any session can read these
    assert(tc.configInt("max_retries") == 5);
    assert(tc.configBool("skip_revert_confirm") == true);
    assert(tc.configStr("inject_serial") == "FAKE_SN_12345");

    // Per-session override
    tc.setConfig("max_retries", "enterprise_test", ConfigValue(int64_t{10}));
    assert(tc.configInt("max_retries", "enterprise_test") == 10); // overridden
    assert(tc.configInt("max_retries", "opal_test") == 5);         // falls back

    tc.disable();
    std::cout << "  Global and per-session config verified\n";
}

int main(int argc, char* argv[]) {
    std::string device = (argc > 1) ? argv[1] : "/dev/nvme0";

    libsed::initialize();

    scenario_global_config();
    scenario_timeout_workaround();
    scenario_custom_callback();
    scenario_fake_discovery();

    // These need a real device
    if (argc > 1) {
        scenario_sp_busy_retry(device);
        scenario_corrupt_response(device);
    } else {
        std::cout << "\n[Skipping device-dependent scenarios. "
                     "Pass a device path to run them.]\n";
    }

    TestContext::instance().reset();
    libsed::shutdown();

    std::cout << "\n=== All evaluation scenarios complete ===\n";
    return 0;
}
