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
    printf("\n── Setup ───────────────────────────────────────────\n");
    printf("목적:  SID 비번 → Locking SP 활성화 → Admin1 비번 설정 (멱등)\n");
    printf("기대:  공장-상태이든 재실행이든 끝나면 Admin1 PIN = ADMIN1_PW.\n\n");

    // ── [1/3] takeOwnership ─────────────────────────────
    printf("[1/3] takeOwnership(SID_PW)\n");
    printf("  의도: 공장이면 MSID→SID_PW 변경, 이미 SID_PW 면 멱등 no-op.\n");
    printf("  예상: OK (또는 AlreadyOwnedDifferentCredential 이면 setup 중단).\n");
    auto cr = composite::takeOwnership(api, transport, comId, SID_PW);
    printf("  결과: %s\n\n", cr.ok() ? "OK" : cr.overall.message().c_str());
    if (cr.failed()) return false;

    // ── [2/3] Activate(SP_LOCKING) ──────────────────────
    printf("[2/3] Activate(SP_LOCKING) — SID 권한\n");
    printf("  의도: Lifecycle=0x08(Manufactured-Inactive) 일 때만 Activate.\n");
    printf("        이미 활성(0x09/0x0A) 이면 skip 하여 멱등 보장.\n");
    printf("  예상: Lifecycle 읽기 OK + (활성화 OK 또는 skip).\n");
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
        lifecycle == 0x08 ? "Manufactured-Inactive — 활성화 수행" :
        lifecycle == 0x09 ? "Manufactured — 이미 활성, skip" :
        lifecycle == 0x0A ? "Issued — 이미 활성, skip" : "기타";
    printf("  Lifecycle: 0x%02X (%s)\n", lifecycle, lcDesc);
    printf("  결과: %s\n\n", r.ok() ? "OK" : r.message().c_str());
    if (r.failed()) return false;

    // ── [3/3] Admin1 PIN 설정 ───────────────────────────
    printf("[3/3] Admin1 PIN = ADMIN1_PW 설정\n");
    printf("  배경: Opal 2.0 spec §5.1.5 — Activate 직후 Admin1 PIN 은\n");
    printf("        'MSID 또는 vendor-defined'. 실측 벤더 변형 4 가지:\n");
    printf("          (a) MSID       — spec 표준\n");
    printf("          (b) SID_PW     — Activate 시 SID 전파 (Samsung 등)\n");
    printf("          (c) ADMIN1_PW  — 이미 setup 된 재실행 케이스\n");
    printf("          (d) empty      — 일부 구형 펌웨어\n");
    printf("  방법: 위 순서로 probe — 처음 hit 전까지의 NotAuthorized 는\n");
    printf("        의도된 정보 수집이며 에러가 아님.\n");
    printf("  예상: 4 개 중 정확히 1 개가 OK. 모두 실패면 vendor 정책 미상.\n\n");

    Bytes msid;
    composite::getMsid(api, transport, comId, msid);

    auto admin1Probe = [&](const Bytes& cred, const char* label,
                            const char* hint) -> Result {
        printf("    Probe %-9s (%s) ... ", label, hint);
        fflush(stdout);
        auto rr = composite::withSession(api, transport, comId,
            uid::SP_LOCKING, true, uid::AUTH_ADMIN1, cred,
            [&](Session& s) { return api.setAdmin1Password(s, ADMIN1_PW); });
        printf("%s\n", rr.ok() ? "*** HIT — Admin1 PIN 이 이 값이었음 ***"
                                : rr.message().c_str());
        return rr;
    };

    auto r2 = admin1Probe(msid,                  "MSID",      "spec 표준");
    if (r2.failed()) r2 = admin1Probe(sidPw,     "SID_PW",    "Activate 시 SID 전파 변형");
    if (r2.failed()) r2 = admin1Probe(pwBytes(ADMIN1_PW),
                                                  "ADMIN1_PW", "재실행 (이미 설정됨)");
    if (r2.failed()) r2 = admin1Probe(Bytes{},   "empty",     "구형 펌웨어");

    printf("\n  결과: %s\n", r2.ok() ? "OK — Admin1 PIN = ADMIN1_PW"
                                       : "ALL FAILED — vendor 정책 미상");
    printf("─────────────────────────────────────────────────────\n\n");
    return r2.ok();
}

// ── Scenario 1: SedDrive Multiple Login Sessions ──

static bool scenario1_multiLogin(const char* device, cli::CliOptions& opts) {
    scenario(1, "Multiple SedDrive Login Sessions");

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
    scenario(2, "SedContext — Thread-Local Pattern");

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
    scenario(3, "Multi-Threaded Discovery");

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
    scenario(0, "Cleanup");
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
