/// @file 06_activate_locking.cpp
/// @brief Activate Locking SP — Enable Drive Encryption
///
/// TCG SPEC CONTEXT:
/// The Locking SP starts in "Manufactured-Inactive" state. Before you can
/// configure locking ranges, set user passwords, or use MBR shadow, you
/// must "activate" it. Activation transitions the Locking SP lifecycle:
///
///   Manufactured-Inactive → Active
///
/// This is done from an Admin SP session authenticated as SID:
///   Activate(SP_LOCKING)  — method on the Locking SP row in Admin SP
///
/// After activation:
///   - Locking ranges become configurable
///   - Admin1 authority exists (initially no password)
///   - User1..User9 authorities exist (disabled by default)
///   - MBR shadow table is available
///   - DataStore table is available
///
/// IMPORTANT: Activation is one-way. You cannot deactivate the Locking SP.
/// To return to Manufactured-Inactive, you must RevertSP from Admin SP.
///
/// This example takes ownership, activates, verifies, then reverts.
///
/// API LAYER: EvalApi + SedDrive
/// PREREQUISITES: 01-05
///
/// Usage: ./06_activate_locking /dev/nvmeX [--dump]

#include "example_common.h"

static std::string SID_PW;
static std::string ADMIN1_PW;

// ── Scenario 1: Activate Locking SP step-by-step ──

static bool scenario1_activateEval(std::shared_ptr<ITransport> transport,
                                    uint16_t comId) {
    scenarioIntent(1, "Activate Locking SP (EvalApi)",
        { "factory-state 드라이브에서 SID 권한으로 Activate(SP_LOCKING)",
          "을 호출해 Locking SP 를 Manufactured-Inactive→Active 로 전환.",
          "Activate 직후 Admin1 PIN 의 vendor 정책을 probe 하여",
          "Admin1 비번을 ADMIN1_PW 로 정착." },
        { "Read MSID OK",
          "Take ownership OK",
          "SID 세션 OK",
          "Lifecycle 읽기 OK (0x08 Manufactured-Inactive 또는 이미 활성)",
          "Activate(SP_LOCKING) OK (또는 이미 활성이면 skip 멱등)",
          "Lifecycle = 0x09 (Active) 로 전환됨",
          "Discovery 의 lockingEnabled = true",
          "Admin1 cred probe 로 hit (MSID/SID_PW/empty 중 하나)",
          "Set Admin1 password OK" });

    EvalApi api;

    // Pre-check: what does Discovery say about Locking?
    DiscoveryInfo info;
    api.discovery0(transport, info);
    printf("    Before activation:\n");
    printf("      Locking present:  %s\n", info.lockingPresent ? "yes" : "no");
    printf("      Locking enabled:  %s\n", info.lockingEnabled ? "yes" : "no");

    // Step 1: Take ownership
    Bytes msid;
    auto cr = composite::getMsid(api, transport, comId, msid);
    step(1, "Read MSID", cr.overall);
    if (cr.failed()) return false;

    cr = composite::takeOwnership(api, transport, comId, SID_PW);
    step(2, "Take ownership (SID=" + SID_PW + ")", cr.overall);
    if (cr.failed()) return false;

    // Step 2: Check Locking SP lifecycle before activation
    {
        Session session(transport, comId);
        StartSessionResult ssr;
        Bytes pw = pwBytes(SID_PW);
        auto r = api.startSessionWithAuth(session, uid::SP_ADMIN, true,
                                           uid::AUTH_SID, pw, ssr);
        step(3, "SID session to Admin SP", r);
        if (r.failed()) return false;

        uint8_t lifecycle = 0;
        r = api.getSpLifecycle(session, uid::SP_LOCKING, lifecycle);
        step(4, "Get Locking SP lifecycle", r);
        printf("    Lifecycle: %u (%s)\n", lifecycle,
               lifecycle == 8 ? "Manufactured-Inactive" :
               lifecycle == 9 ? "Active" : "Unknown");

        // Step 3: Activate (only if Manufactured-Inactive — 멱등 보장)
        if (lifecycle == 8) {
            r = api.activate(session, uid::SP_LOCKING);
            step(5, "Activate(SP_LOCKING)", r);
            if (r.failed()) { api.closeSession(session); return false; }
        } else {
            step(5, "Activate (already active — skip 멱등)", true);
        }

        // Verify lifecycle changed
        r = api.getSpLifecycle(session, uid::SP_LOCKING, lifecycle);
        step(6, "Verify lifecycle after activation", r);
        printf("    Lifecycle: %u (%s)\n", lifecycle,
               lifecycle == 9 ? "Active" : "not Active?!");

        api.closeSession(session);
    }

    // Step 4: Verify via Discovery — lockingEnabled should now be true
    api.discovery0(transport, info);
    step(7, "Discovery after activation", info.lockingEnabled);
    printf("    Locking enabled: %s\n", info.lockingEnabled ? "yes" : "no");

    // Step 5: Admin1 PIN 은 spec §5.1.5 상 "MSID 또는 vendor-defined".
    //         (a) MSID, (b) SID_PW, (c) empty 순서로 probe 해 hit 하는 cred 로
    //         Admin1 PIN = ADMIN1_PW 설정. 처음 hit 전의 NotAuthorized 는
    //         정보-수집 목적이며 에러가 아님.
    Bytes sidPw = pwBytes(SID_PW);
    auto admin1Probe = [&](const Bytes& cred, const char* label) -> Result {
        Session s(transport, comId);
        StartSessionResult ssr;
        auto rr = api.startSessionWithAuth(s, uid::SP_LOCKING, true,
                                            uid::AUTH_ADMIN1, cred, ssr);
        printf("    Admin1 probe [%-7s] -> %s\n", label,
               rr.ok() ? "*** HIT ***" : rr.message().c_str());
        if (rr.ok()) {
            rr = api.setAdmin1Password(s, ADMIN1_PW);
            api.closeSession(s);
        }
        return rr;
    };
    auto r = admin1Probe(msid, "MSID");
    if (r.failed()) r = admin1Probe(sidPw, "SID_PW");
    if (r.failed()) r = admin1Probe(Bytes{}, "empty");
    step(8, "Admin1 cred probe (4 단계 fallback)", r);
    step(9, "Set Admin1 password", r);

    return r.ok();
}

// ── Scenario 2: Revert back to factory ──

static bool scenario2_revert(std::shared_ptr<ITransport> transport,
                              uint16_t comId) {
    scenarioIntent(2, "Revert to Factory",
        { "AdminSP.Revert() 로 Locking SP 활성화/Admin1 비번/User 설정",
          "을 모두 무효화하고 드라이브를 factory state 로 복원." },
        { "RevertToFactory OK, 후속 Discovery 의 lockingEnabled = false" });

    EvalApi api;
    auto cr = composite::revertToFactory(api, transport, comId, SID_PW);
    step(1, "RevertToFactory", cr.overall);

    // Verify
    DiscoveryInfo info;
    api.discovery0(transport, info);
    printf("    Locking enabled after revert: %s\n",
           info.lockingEnabled ? "yes (unexpected!)" : "no (factory state)");

    return cr.ok();
}

// ── Scenario 3: SedDrive one-liner ──

static bool scenario3_facade(const char* device, cli::CliOptions& opts) {
    scenarioIntent(3, "SedDrive::activateLocking()",
        { "scenario 1 의 take_own + activate 흐름을 SedDrive facade 한",
          "줄로 축약. 내부적으로 lifecycle 체크 + 자동 cleanup 수행." },
        { "takeOwnership OK",
          "activateLocking OK",
          "revert OK" });

    SedDrive drive(device);
    if (opts.dump) drive.enableDump(std::cerr, opts.dumpLevel);
    drive.query();

    auto r = drive.takeOwnership(SID_PW);
    step(1, "takeOwnership", r);
    if (r.failed()) return false;

    r = drive.activateLocking(SID_PW);
    step(2, "activateLocking", r);

    // Clean up
    r = drive.revert(SID_PW);
    step(3, "revert (cleanup)", r);

    return true;
}

int main(int argc, char* argv[]) {
    cli::CliOptions opts;
    auto transport = initTransport(argc, argv, opts,
        "Activate Locking SP — enable drive encryption features");
    if (!transport) return 1;

    SID_PW = getPassword(opts);
    ADMIN1_PW = SID_PW + "_Admin1";

    banner("06: Activate Locking SP");
    printf("  Drive must be in factory state.\n");
    printf("  This example takes ownership, activates, then reverts.\n\n");

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    bool ok = true;
    ok &= scenario1_activateEval(transport, info.baseComId);
    ok &= scenario2_revert(transport, info.baseComId);
    ok &= scenario3_facade(opts.device.c_str(), opts);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
