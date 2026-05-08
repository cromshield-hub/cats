/// @file 18_fault_injection.cpp
/// @brief Fault Injection — FaultBuilder and TestContext
///
/// TCG SPEC CONTEXT:
/// Testing SED software requires verifying behavior under failure conditions.
/// What happens when:
///   - A packet is corrupted in transit?
///   - The TPer returns an unexpected error?
///   - A session times out mid-operation?
///   - Authentication fails after a partial setup?
///
/// libsed's debug layer provides:
///
///   FaultBuilder: Fluent API to define fault injection rules
///     .at(FaultPoint)   — where in the protocol to inject
///     .returnError()    — make a step return an error code
///     .corrupt()        — corrupt payload bytes
///     .delay()          — add latency
///     .drop()           — silently drop the packet
///     .replaceWith()    — substitute a different payload
///     .callback()       — custom logic
///     .once()/.times(n)/.always() — how many times to fire
///
///   TestContext: Manages armed fault rules and test state
///     .arm(rule)        — activate a fault rule
///     .disarmAll()      — clear all rules
///
///   FaultPoints (24 injection points):
///     BeforeSend, AfterSend, BeforeRecv, AfterRecv,
///     BeforeStartSession, AfterStartSession,
///     BeforeCloseSession, BeforeAuthenticate, etc.
///
/// API LAYER: EvalApi + Debug (FaultBuilder, TestContext)
/// PREREQUISITES: 01-05, 16 (EvalApi understanding)
///
/// Usage: ./18_fault_injection /dev/nvmeX [--dump]

#include "example_common.h"
#include "libsed/debug/fault_builder.h"
#include "libsed/debug/test_context.h"

using namespace libsed::debug;

// ── Scenario 1: Simulate Transport Failure ──

static bool scenario1_transportFailure(std::shared_ptr<ITransport> transport,
                                        uint16_t comId) {
    scenario(1, "Simulate Transport Send Failure");
    printf("  Intent:   FaultBuilder 로 BeforeIfSend 시점에 TransportSendFailed 를\n");
    printf("            한 번만(.once()) 박아둔 뒤 첫 호출이 실패하고 두 번째는 정상\n");
    printf("            작동하는지 검증. fault 의 격리 (one-shot) 가 핵심.\n");
    printf("  Expected: 2 단계:\n");
    printf("            1) Properties 첫 호출 → FAIL (의도된 fault — 정답)\n");
    printf("            2) disarmAll 후 재호출 → OK (정상 복원)\n\n");

    EvalApi api;

    // Arm a fault: make the next IF-SEND return an error
    FaultBuilder builder;
    auto rule = builder
        .at(FaultPoint::BeforeIfSend)
        .returnError(ErrorCode::TransportSendFailed)
        .once()
        .build();

    auto ruleId = TestContext::instance().armFault(rule);
    printf("    Armed fault rule: %s\n", ruleId.c_str());

    // Intent: armFault 가 BeforeIfSend 에 TransportSendFailed 를 박아두었으므로
    //         이 호출은 실패가 정답.
    PropertiesResult props;
    auto r = api.exchangeProperties(transport, comId, props);
    stepExpect(1, "Properties with injected send failure", Expect::Failure, r);

    // Intent: disarm 후 재시도 → 정상 동작 복원되어 성공이 정답.
    TestContext::instance().disarmAllFaults();
    r = api.exchangeProperties(transport, comId, props);
    stepExpect(2, "Properties after disarm", Expect::Success, r);

    return true;
}

// ── Scenario 2: Corrupt Payload ──

static bool scenario2_corruptPayload(std::shared_ptr<ITransport> transport,
                                      uint16_t comId) {
    scenario(2, "Corrupt Outgoing Payload");
    printf("  Intent:   .corrupt(offset=30, mask=0xFF) 로 송출 직전에 byte 31 을\n");
    printf("            XOR 변조. TPer 가 malformed packet 으로 거부하는지 확인.\n");
    printf("  Expected: 1 단계:\n");
    printf("            1) Properties (변조된 payload) → FAIL (TPer 거부 — 정답)\n\n");

    EvalApi api;

    // Arm: corrupt the send payload (flip a byte at offset 30)
    FaultBuilder builder;
    auto rule = builder
        .at(FaultPoint::BeforeIfSend)
        .corrupt(30, 0xFF)  // XOR byte at offset 30 with 0xFF
        .once()
        .build();

    TestContext::instance().armFault(rule);

    // Intent: payload byte 가 변조되어 송출되므로 TPer 가 거부 (또는 parse 실패).
    //         실패가 정답.
    PropertiesResult props;
    auto r = api.exchangeProperties(transport, comId, props);
    stepExpect(1, "Properties with corrupted payload", Expect::Failure, r);

    TestContext::instance().disarmAllFaults();
    return true;
}

// ── Scenario 3: Multi-fire Faults ──

static bool scenario3_multiFire(std::shared_ptr<ITransport> transport,
                                 uint16_t comId) {
    scenario(3, "Multi-Fire Fault (fail N times)");
    printf("  Intent:   .times(2) 로 처음 2 회만 실패, 3 회째는 자동 통과.\n");
    printf("            retry 로직의 fault 회복성 테스트 (2회 실패 후 성공 시나리오).\n");
    printf("  Expected: 3 단계:\n");
    printf("            1) Properties #1 → FAIL (fault rule fire 1/2 — 정답)\n");
    printf("            2) Properties #2 → FAIL (fault rule fire 2/2 — 정답)\n");
    printf("            3) Properties #3 → OK   (rule 소진 후 정상 — 정답)\n\n");

    EvalApi api;

    // Arm: fail the first 2 attempts, then succeed
    FaultBuilder builder;
    auto rule = builder
        .at(FaultPoint::BeforeIfSend)
        .returnError(ErrorCode::TransportSendFailed)
        .times(2)  // Fail first 2 calls only
        .build();

    TestContext::instance().armFault(rule);

    // Intent: rule 이 처음 2회만 실패시키도록 박혔으므로 — 1·2번째는 실패가 정답,
    //         3번째는 성공이 정답.
    PropertiesResult props;
    for (int i = 0; i < 3; i++) {
        auto r = api.exchangeProperties(transport, comId, props);
        char label[64];
        snprintf(label, sizeof(label), "Attempt %d", i + 1);
        Expect expect = (i < 2) ? Expect::Failure : Expect::Success;
        stepExpect(i + 1, label, expect, r);
    }

    TestContext::instance().disarmAllFaults();
    return true;
}

// ── Scenario 4: Fault Callback ──

static bool scenario4_callback(std::shared_ptr<ITransport> transport,
                                uint16_t comId) {
    scenario(4, "Custom Fault Callback");
    printf("  Intent:   .callback() 으로 송출 직전 payload 를 가로채 로그만 남기고\n");
    printf("            그대로 통과(Success 반환). 비파괴 모니터링/추적 패턴.\n");
    printf("            .times(3) 으로 첫 3 번의 send 만 콜백 발화.\n");
    printf("  Expected: 1 단계 + 콜백 카운트:\n");
    printf("            1) callCount > 0 (Properties + StartSession + CloseSession 의\n");
    printf("               각 send 마다 콜백 발화)\n\n");

    EvalApi api;
    int callCount = 0;

    // Arm: custom callback that logs and allows the call
    FaultBuilder builder;
    auto rule = builder
        .at(FaultPoint::BeforeIfSend)
        .callback([&callCount](Bytes& payload) -> Result {
            callCount++;
            printf("    [Callback] Intercepted send #%d, payload %zu bytes\n",
                   callCount, payload.size());
            // Return Success to allow the call to proceed
            return ErrorCode::Success;
        })
        .times(3)
        .build();

    TestContext::instance().armFault(rule);

    // Run some operations — callback will fire on each send
    PropertiesResult props;
    api.exchangeProperties(transport, comId, props);

    Session session(transport, comId);
    StartSessionResult ssr;
    api.startSession(session, uid::SP_ADMIN, false, ssr);
    api.closeSession(session);

    step(1, "Callback fired on sends", callCount > 0);
    printf("    Total callback invocations: %d\n", callCount);

    TestContext::instance().disarmAllFaults();
    return true;
}

int main(int argc, char* argv[]) {
    cli::CliOptions opts;
    auto transport = initTransport(argc, argv, opts,
        "Fault Injection — FaultBuilder and TestContext");
    if (!transport) return 1;

    banner("18: Fault Injection");

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    bool ok = true;
    ok &= scenario1_transportFailure(transport, info.baseComId);
    ok &= scenario2_corruptPayload(transport, info.baseComId);
    ok &= scenario3_multiFire(transport, info.baseComId);
    ok &= scenario4_callback(transport, info.baseComId);

    // Always clean up
    TestContext::instance().disarmAllFaults();

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
