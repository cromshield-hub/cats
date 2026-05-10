/// @file 02_properties.cpp
/// @brief Properties Exchange — Host/TPer Negotiation
///
/// TCG SPEC CONTEXT:
/// Before any session can begin, the Host and TPer must agree on
/// communication parameters via a "Properties Exchange." This is a
/// Session Manager (SM) method call (MethodUID 0xFF01) sent to the
/// SM_UID (0x0000000000FF) — it does NOT require an open session.
///
/// The Host sends its proposed parameters:
///   MaxComPacketSize, MaxResponseComPacketSize, MaxPacketSize,
///   MaxIndTokenSize, MaxPackets, MaxSubpackets, MaxMethods
///
/// The TPer responds with its own parameters AND echoes back the
/// Host parameters (possibly adjusted downward). Both sides must
/// use the MINIMUM of proposed values for actual communication.
///
/// This negotiation happens once per ComID, before StartSession.
/// If you skip it, you'll use defaults — which may be too small.
///
/// API LAYER: EvalApi for step-by-step control, SedDrive for comparison.
/// PREREQUISITES: 01_hello_discovery (understand ComID from Discovery)
///
/// Usage: ./02_properties /dev/nvmeX [--dump]

#include "example_common.h"

// ── Scenario 1: Properties Exchange via EvalApi ──
//
// This shows the actual protocol step. You'll see exactly what
// parameters the TPer reports and what it accepts from the Host.

static bool scenario1_evalProperties(std::shared_ptr<ITransport> transport,
                                      uint16_t comId) {
    scenarioIntent(1, "Properties Exchange via EvalApi",
        { "SM 메서드 0xFF01 을 직접 보내고 TPer 가 reporting 하는 limit 을 받는다.",
          "모든 후속 세션 통신은 이 값에 맞춰서 패킷이 만들어진다." },
        { "exchangeProperties() 성공",
          "TPer Properties (MaxComPacketSize / MaxPacketSize / MaxIndTokenSize 등) 출력" });

    EvalApi api;
    PropertiesResult props;

    // exchangeProperties() sends SM method 0xFF01 and parses the response.
    // It needs the ComID from Discovery (default 0x07FE for Opal).
    auto r = api.exchangeProperties(transport, comId, props);
    step(1, "EvalApi::exchangeProperties()", r);
    if (r.failed()) return false;

    // TPer-reported properties: what the drive can handle
    printf("\n  TPer Properties (what the drive supports):\n");
    printf("    MaxComPacketSize:   %u\n", props.tperMaxComPacketSize);
    printf("    MaxPacketSize:      %u\n", props.tperMaxPacketSize);
    printf("    MaxIndTokenSize:    %u\n", props.tperMaxIndTokenSize);
    printf("    MaxAggTokenSize:    %u\n", props.tperMaxAggTokenSize);
    printf("    MaxSubPackets:      %u\n", props.tperMaxSubPackets);
    printf("    MaxMethods:         %u\n", props.tperMaxMethods);

    return true;
}

// ── Scenario 2: Custom Properties Request ──
//
// You can propose non-default values to test TPer behavior.
// Some TPers will accept larger sizes, others will clamp down.

static bool scenario2_customProperties(std::shared_ptr<ITransport> transport,
                                        uint16_t comId) {
    scenarioIntent(2, "Custom Properties Request",
        { "Host 가 64KB 같은 큰 값을 제안해보고, TPer 가 받아주는지 / clamp 하는지",
          "관찰 — 펌웨어별 한계를 확인하는 진단 절차." },
        { "exchangePropertiesCustom() 성공",
          "TPer 의 응답이 accepted 인지 clamped 인지 표시" });

    EvalApi api;
    PropertiesResult props;

    // Request larger-than-default values to see what the TPer accepts
    auto r = api.exchangePropertiesCustom(transport, comId,
        65536,   // MaxComPacketSize (default 2048)
        65512,   // MaxPacketSize
        65488,   // MaxIndTokenSize
        props);
    step(1, "Custom properties (64KB request)", r);
    if (r.failed()) return false;

    printf("\n  TPer response to 64KB request:\n");
    printf("    MaxComPacketSize:   %u %s\n",
           props.tperMaxComPacketSize,
           props.tperMaxComPacketSize >= 65536 ? "(accepted!)" : "(clamped)");

    // Note: The actual communication sizes used should be
    // min(host_proposed, tper_reported) for each parameter.

    return true;
}

// ── Scenario 3: SedDrive handles it automatically ──
//
// When you use SedDrive::query(), Properties Exchange is done
// internally. The results are cached and used for all subsequent
// session communication.

static bool scenario3_facadeAuto(const char* device, cli::CliOptions& opts) {
    scenarioIntent(3, "SedDrive Automatic Properties",
        { "SedDrive::query() 가 내부적으로 Properties 를 한 번 교환하고",
          "결과를 cache — 호출자는 maxComPacketSize() 같은 accessor 만 쓰면 된다." },
        { "SedDrive::query() 성공",
          "drive.comId() / drive.maxComPacketSize() 출력 (모두 cached)" });

    SedDrive drive(device);
    if (opts.dump) drive.enableDump(std::cerr, opts.dumpLevel);

    auto r = drive.query();
    step(1, "SedDrive::query() (includes Properties)", r);
    if (r.failed()) return false;

    printf("\n  Negotiated parameters available via SedDrive:\n");
    printf("    ComID:              0x%04X\n", drive.comId());
    printf("    MaxComPacketSize:   %u\n", drive.maxComPacketSize());

    return true;
}

int main(int argc, char* argv[]) {
    cli::CliOptions opts;
    auto transport = initTransport(argc, argv, opts,
        "Properties Exchange — Host/TPer parameter negotiation");
    if (!transport) return 1;

    banner("02: Properties Exchange");

    // We need the ComID from Discovery first
    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) {
        printf("Discovery failed: %s\n", r.message().c_str());
        return 1;
    }
    printf("  Using ComID 0x%04X from Discovery\n", info.baseComId);

    bool ok = true;
    ok &= scenario1_evalProperties(transport, info.baseComId);
    ok &= scenario2_customProperties(transport, info.baseComId);
    ok &= scenario3_facadeAuto(opts.device.c_str(), opts);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
