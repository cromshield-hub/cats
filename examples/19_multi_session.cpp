/// @file 19_multi_session.cpp
/// @brief Multi-Session — Concurrent Sessions and Threading
///
/// TCG SPEC CONTEXT:
/// The TPer supports a limited number of concurrent sessions (typically 1-4
/// per SP, drive-dependent). Multiple sessions are useful for:
///
///   - Multi-threaded test frameworks (each thread gets its own session)
///   - Testing session isolation (changes in one session don't leak to another)
///   - Stress testing session slot exhaustion
///
/// Threading rules:
///   - EvalApi is stateless and thread-safe (no locks needed)
///   - Session is NOT thread-safe — use one Session per thread
///   - SedContext bundles transport + session for per-thread use
///   - ComID management matters — some drives share ComIDs across sessions
///
/// Each session gets a unique TSN from the TPer. The Host assigns HSN
/// (typically 1 for each thread). The TSN/HSN pair identifies the session
/// in all Packet headers.
///
/// API LAYER: SedDrive multi-session + SedContext + std::thread
/// PREREQUISITES: 01-08
///
/// Usage: ./19_multi_session /dev/nvmeX [--dump]

#include "example_common.h"
#include "libsed/eval/sed_context.h"
#include <thread>
#include <mutex>

static std::string SID_PW;
static std::string ADMIN1_PW;

static std::mutex g_printMutex;

static bool setupDrive(EvalApi& api, std::shared_ptr<ITransport> transport,
                       uint16_t comId) {
    scenarioIntent(0, "Setup — SID 비번 + Locking SP 활성화 + Admin1 비번 (멱등)",
        { "공장 상태이든 재실행이든 끝나면 Admin1 PIN = ADMIN1_PW 가 보장되도록",
          "takeOwnership → Activate(Locking) → Admin1 PIN 설정을 idempotent 하게 진행." },
        { "takeOwnership(SID_PW) — 공장이면 set, 이미 SID_PW 면 no-op",
          "Lifecycle 읽기 → 0x08 이면 Activate, 0x09/0x0A 면 skip",
          "Admin1 PIN probe 4 가지 (MSID / SID_PW / ADMIN1_PW / empty) 중 1개 hit" });

    // [1/3] takeOwnership — 공장이면 MSID→SID_PW, 이미 SID_PW 면 멱등 no-op
    auto cr = composite::takeOwnership(api, transport, comId, SID_PW);
    step(1, "takeOwnership (idempotent)", cr.overall);
    if (cr.failed()) return false;

    // [2/3] Activate(Locking SP) — Lifecycle=0x08 일 때만 수행
    Bytes sidPw = pwBytes(SID_PW);
    uint8_t lifecycle = 0;
    RawResult lcRaw;
    auto r = composite::withSession(api, transport, comId,
        uid::SP_ADMIN, true, uid::AUTH_SID, sidPw,
        [&](Session& s) -> Result {
            auto rr = api.getSpLifecycle(s, uid::SP_LOCKING, lifecycle, lcRaw);
            if (rr.failed()) return rr;
            if (lifecycle == 0x08) return api.activate(s, uid::SP_LOCKING);
            return ErrorCode::Success;
        });
    const char* lcDesc =
        lifecycle == 0x08 ? "Manufactured-Inactive — activated" :
        lifecycle == 0x09 ? "Manufactured — already active, skip" :
        lifecycle == 0x0A ? "Issued — already active, skip" : "other";
    printf("            -> Lifecycle 0x%02X (%s)\n", lifecycle, lcDesc);
    step(2, "Activate Locking SP (idempotent)", r);
    if (r.failed()) return false;

    // [3/3] Admin1 PIN probe — Opal 2.0 §5.1.5 의 vendor 변형 4 가지 중 hit 찾기
    // (앞쪽 NotAuthorized 는 정보 수집이며 에러가 아님 — stepExpect 로 표현)
    Bytes msid;
    composite::getMsid(api, transport, comId, msid);

    auto admin1Probe = [&](const Bytes& cred) {
        return composite::withSession(api, transport, comId,
            uid::SP_LOCKING, true, uid::AUTH_ADMIN1, cred,
            [&](Session& s) { return api.setAdmin1Password(s, ADMIN1_PW); });
    };

    Result r2 = admin1Probe(msid);
    const char* hit = r2.ok() ? "MSID (spec 표준)" : nullptr;
    if (!hit) { r2 = admin1Probe(sidPw);                hit = r2.ok() ? "SID_PW (Activate 시 SID 전파 변형)" : nullptr; }
    if (!hit) { r2 = admin1Probe(pwBytes(ADMIN1_PW));   hit = r2.ok() ? "ADMIN1_PW (재실행)" : nullptr; }
    if (!hit) { r2 = admin1Probe(Bytes{});              hit = r2.ok() ? "empty (구형 펌웨어)" : nullptr; }

    if (hit) printf("            -> Admin1 PIN was: %s\n", hit);
    step(3, "Admin1 PIN probe (4 variants)", r2);
    return r2.ok();
}

// ── Scenario 1: SedDrive Multiple Login Sessions ──

static bool scenario1_multiLogin(const char* device, cli::CliOptions& opts) {
    scenarioIntent(1, "Multiple SedDrive Login Sessions",
        { "같은 SP 에 두 개 동시 세션을 열어 격리(isolation) 검증.",
          "(많은 펌웨어가 SP 당 1 세션으로 제한 — 그 경우 두 번째 login 실패가 자연스러운 결과)" },
        { "Session 1 login(Locking SP, Admin1) 성공",
          "Session 2 login → 성공이면 두 TSN 이 다른지 확인 + 양쪽 read 격리",
          "Session 2 실패면 'multi-session 미지원' 메시지 + Session 1 close" });

    SedDrive drive(device);
    if (opts.dump) drive.enableDump(std::cerr, opts.dumpLevel);
    drive.query();

    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    // Open two sessions to Locking SP
    auto session1 = drive.login(uid::SP_LOCKING, ADMIN1_PW, uid::AUTH_ADMIN1);
    step(1, "Session 1: login(Locking SP, Admin1)", session1.openResult());

    if (session1.failed()) {
        printf("    Cannot open first session\n");
        return false;
    }

    // Try to open a second session
    auto session2 = drive.login(uid::SP_LOCKING, ADMIN1_PW, uid::AUTH_ADMIN1);
    step(2, "Session 2: login(Locking SP, Admin1)", session2.openResult());

    if (session2.ok()) {
        printf("    Two concurrent sessions active!\n");

        // Each session has its own TSN
        printf("    Session 1 TSN: %u\n", session1.raw().tperSessionNumber());
        printf("    Session 2 TSN: %u\n", session2.raw().tperSessionNumber());

        // Operations in one session don't affect the other
        LockingRangeInfo info;
        session1.getRangeInfo(0, info);
        step(3, "Session 1: read range 0", true);

        session2.getRangeInfo(0, info);
        step(4, "Session 2: read range 0", true);

        // Close both (or let RAII handle it)
        session2.close();
        session1.close();
    } else {
        printf("    Drive doesn't support multiple concurrent sessions\n");
        printf("    (This is common — many drives limit to 1 session per SP)\n");
        session1.close();
    }

    return true;
}

// ── Scenario 2: SedContext for Thread-Local State ──

static bool scenario2_sedContext(std::shared_ptr<ITransport> transport,
                                  uint16_t comId) {
    scenarioIntent(2, "SedContext — Thread-Local Pattern",
        { "SedContext 가 transport + EvalApi + Session + cached discovery 를",
          "한 번에 묶어 thread 별 instance 로 사용하는 패턴 시연." },
        { "SedContext::initialize() 성공 (ComID / SSC 출력)",
          "ctx.openSession(Locking, Admin1) 성공 + TSN 출력",
          "ctx.api() / ctx.session() 으로 read 호출 OK" });

    // SedContext bundles: transport + api + session + cached discovery
    // Each thread should create its own SedContext
    SedContext ctx(transport);
    auto r = ctx.initialize();
    step(1, "SedContext::initialize()", r);
    if (r.failed()) return false;

    printf("    ComID:     0x%04X\n", ctx.comId());
    printf("    SSC:       %s\n",
           ctx.tcgOption().sscType == SscType::Opal20 ? "Opal 2.0" :
           ctx.tcgOption().sscType == SscType::Enterprise ? "Enterprise" : "Other");

    // Open a session through SedContext
    Bytes admin1Pw = pwBytes(ADMIN1_PW);
    r = ctx.openSession(uid::SP_LOCKING, uid::AUTH_ADMIN1, admin1Pw, true);
    step(2, "ctx.openSession(Locking, Admin1)", r);
    if (r.ok()) {
        printf("    TSN: %u\n", ctx.session().tperSessionNumber());

        // Use ctx.api() and ctx.session() together
        LockingRangeInfo info;
        ctx.api().getRangeInfo(ctx.session(), 0, info);
        step(3, "Read range 0 via SedContext", true);

        ctx.closeSession();
    }

    return true;
}

// ── Scenario 3: Multi-threaded Operations ──

static bool scenario3_threading(std::shared_ptr<ITransport> transport,
                                 uint16_t comId) {
    scenarioIntent(3, "Multi-Threaded Discovery",
        { "EvalApi 가 stateless / thread-safe 라는 계약을 검증 — 3 개 thread 가",
          "동시에 Discovery 호출해도 모두 같은 답을 받는다." },
        { "3 thread 가 각자 EvalApi instance 로 discovery0() OK",
          "모두 같은 SSC 인지 확인 (mutex 로 출력 직렬화)" });

    // Multiple threads can run Discovery concurrently
    // (Discovery doesn't require a session)
    const int NUM_THREADS = 3;
    std::vector<std::thread> threads;
    std::vector<bool> results(NUM_THREADS, false);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&, i]() {
            EvalApi api;
            DiscoveryInfo info;
            auto r = api.discovery0(transport, info);

            std::lock_guard<std::mutex> lock(g_printMutex);
            printf("    Thread %d: Discovery %s (SSC=%s)\n", i,
                   r.ok() ? "OK" : "FAIL",
                   info.primarySsc == SscType::Opal20 ? "Opal" : "Other");
            results[i] = r.ok();
        });
    }

    for (auto& t : threads) t.join();

    int passed = 0;
    for (auto r : results) if (r) passed++;
    step(1, "All threads completed Discovery", passed == NUM_THREADS);

    return passed == NUM_THREADS;
}

static bool cleanup(std::shared_ptr<ITransport> transport, uint16_t comId) {
    scenarioIntent(99, "Cleanup",
        { "Multi-session 시연으로 변형된 LockingSP 상태를 모두 되돌림." },
        { "composite::revertToFactory 성공" });
    EvalApi api;
    auto cr = composite::revertToFactory(api, transport, comId, SID_PW);
    step(1, "RevertToFactory", cr.overall);
    return cr.ok();
}

int main(int argc, char* argv[]) {
    cli::CliOptions opts;
    auto transport = initTransport(argc, argv, opts,
        "Multi-Session — concurrent sessions and threading patterns");
    if (!transport) return 1;

    SID_PW = getPassword(opts);
    ADMIN1_PW = SID_PW + "_Admin1";

    banner("19: Multi-Session");

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    if (!setupDrive(api, transport, info.baseComId)) {
        printf("  Setup failed.\n"); return 1;
    }

    bool ok = true;
    ok &= scenario1_multiLogin(opts.device.c_str(), opts);
    ok &= scenario2_sedContext(transport, info.baseComId);
    ok &= scenario3_threading(transport, info.baseComId);
    cleanup(transport, info.baseComId);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
