/// @file 08_user_management.cpp
/// @brief User Management — Enable Users, Set Passwords, Assign to Ranges
///
/// TCG SPEC CONTEXT:
/// Opal 2.0 supports up to 4 Admin and 9 User authorities in the Locking SP.
/// After activation, only Admin1 is usable (with empty or set password).
/// Users (User1-User9) exist but are disabled by default.
///
/// To let a user lock/unlock a specific range:
///   1. Enable the User authority (set Enabled=true in Authority table)
///   2. Set the User's password (in C_PIN table)
///   3. Add User to the ACE (Access Control Element) for the range
///      - ACE_Locking_Range1_Set_RdLocked: controls who can read-unlock
///      - ACE_Locking_Range1_Set_WrLocked: controls who can write-unlock
///
/// This separation of duties is key to TCG's security model:
///   - Admin1: configures ranges and manages users
///   - User1: can only lock/unlock ranges they're assigned to
///   - SID: ultimate authority, can revert everything
///
/// API LAYER: EvalApi + SedDrive
/// PREREQUISITES: 01-07
///
/// Usage: ./08_user_management /dev/nvmeX [--dump]

#include "example_common.h"

static std::string SID_PW;
static std::string ADMIN1_PW;
static std::string USER1_PW;

static bool setupDrive(EvalApi& api, std::shared_ptr<ITransport> transport,
                       uint16_t comId) {
    printf("\n── Setup ───────────────────────────────────────────\n");
    printf("목적:  takeOwnership + Activate(SP_LOCKING) + Admin1 PIN = ADMIN1_PW\n");
    printf("기대:  공장-상태이든 재실행이든 끝나면 scenario 들이 Admin1/User1 권한\n");
    printf("       으로 진입 가능한 상태.\n\n");

    // [1] takeOwnership
    printf("[1/3] takeOwnership(SID_PW) — 멱등\n");
    auto cr = composite::takeOwnership(api, transport, comId, SID_PW);
    printf("      → %s\n", cr.ok() ? "OK" : cr.overall.message().c_str());
    if (cr.failed()) return false;

    // [2] Lifecycle-aware Activate (이미 활성이면 skip)
    printf("[2/3] Activate(SP_LOCKING) — Lifecycle 체크 후 멱등 실행\n");
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
    printf("      Lifecycle=0x%02X (%s) → %s\n", lifecycle,
           lifecycle == 0x08 ? "Manufactured-Inactive — 활성화" :
           lifecycle == 0x09 ? "Manufactured — skip" : "기타",
           r.ok() ? "OK" : r.message().c_str());
    if (r.failed()) return false;

    // [3] Admin1 cred probe (Opal §5.1.5: MSID 또는 vendor-defined)
    printf("[3/3] Admin1 PIN = ADMIN1_PW (4-단계 cred probe)\n");
    Bytes msid;
    composite::getMsid(api, transport, comId, msid);
    auto admin1Probe = [&](const Bytes& cred, const char* label) -> Result {
        printf("      Probe [%-9s] ... ", label); fflush(stdout);
        auto rr = composite::withSession(api, transport, comId,
            uid::SP_LOCKING, true, uid::AUTH_ADMIN1, cred,
            [&](Session& s) { return api.setAdmin1Password(s, ADMIN1_PW); });
        printf("%s\n", rr.ok() ? "*** HIT ***" : rr.message().c_str());
        return rr;
    };
    auto r2 = admin1Probe(msid, "MSID");
    if (r2.failed()) r2 = admin1Probe(sidPw, "SID_PW");
    if (r2.failed()) r2 = admin1Probe(pwBytes(ADMIN1_PW), "ADMIN1_PW");
    if (r2.failed()) r2 = admin1Probe(Bytes{}, "empty");
    printf("      결과: %s\n", r2.ok() ? "OK — Admin1 PIN = ADMIN1_PW"
                                          : "ALL FAILED");
    printf("─────────────────────────────────────────────────────\n\n");
    return r2.ok();
}

// ── Scenario 1: Enable User1, Set Password, Assign to Range 1 ──
//
// Full user setup flow as Admin1.

static bool scenario1_setupUser(std::shared_ptr<ITransport> transport,
                                 uint16_t comId) {
    scenarioIntent(1, "Setup User1 — Enable, Password, ACE",
        { "Admin1 권한으로 LockingSP 에 들어가 User1 을 활성화하고",
          "비번을 설정한 뒤 Range 1 의 lock/unlock ACE 에 등록." },
        { "Range 1 구성 (start=0, length=1024, both lock 활성) OK",
          "enableUser(User1) — Authority.Enabled = true OK",
          "isUserEnabled(User1) → true 검증 OK",
          "setUserPassword(User1, USER1_PW) OK",
          "ACE_Locking_Range1_Set_Rd/WrLocked 에 User1 추가 OK" });

    EvalApi api;
    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, admin1Pw,
        [&](Session& session) -> Result {

            // Configure Range 1 first
            auto r2 = api.setRange(session, 1, 0, 1024, true, true);
            step(1, "Configure Range 1", r2);
            if (r2.failed()) return r2;

            // Enable User1 authority
            // Under the hood: Set(Authority_User1, Enabled=true)
            r2 = api.enableUser(session, 1);
            step(2, "Enable User1", r2);
            if (r2.failed()) return r2;

            // Verify User1 is enabled
            bool enabled = false;
            r2 = api.isUserEnabled(session, 1, enabled);
            step(3, "Verify User1 enabled", r2);
            printf("    User1 enabled: %s\n", enabled ? "yes" : "no");

            // Set User1 password
            r2 = api.setUserPassword(session, 1, USER1_PW);
            step(4, "Set User1 password", r2);

            // Assign User1 to Range 1 ACE
            // This adds User1 to ACE_Locking_Range1_Set_RdLocked and
            // ACE_Locking_Range1_Set_WrLocked, allowing User1 to
            // lock/unlock Range 1.
            r2 = api.assignUserToRange(session, 1, 1);  // userId=1, rangeId=1
            step(5, "Assign User1 to Range 1", r2);

            return ErrorCode::Success;
        });

    return r.ok();
}

// ── Scenario 2: User1 locks and unlocks Range 1 ──
//
// Now we authenticate as User1 (not Admin1) and control Range 1.

static bool scenario2_userLockUnlock(std::shared_ptr<ITransport> transport,
                                      uint16_t comId) {
    scenarioIntent(2, "User1 Lock/Unlock Range 1",
        { "scenario 1 에서 등록한 User1 의 권한이 실제로 작동하는지",
          "User1 비번으로 직접 인증 후 lock→verify→unlock→verify." },
        { "User1: Range 1 lock (Rd/WrLocked = true) OK",
          "getRangeInfo → ReadLocked=yes, WriteLocked=yes",
          "User1: Range 1 unlock (Rd/WrLocked = false) OK",
          "getRangeInfo → ReadLocked=no, WriteLocked=no" });

    EvalApi api;
    Bytes user1Pw = pwBytes(USER1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_USER1, user1Pw,
        [&](Session& session) -> Result {

            // User1 locks Range 1
            auto r2 = api.setRangeLock(session, 1, true, true);
            step(1, "User1: Lock Range 1", r2);
            if (r2.failed()) return r2;

            // Verify locked
            LockingRangeInfo info;
            r2 = api.getRangeInfo(session, 1, info);
            step(2, "Verify locked", r2);
            printf("    ReadLocked=%s WriteLocked=%s\n",
                   info.readLocked ? "yes" : "no",
                   info.writeLocked ? "yes" : "no");

            // User1 unlocks Range 1
            r2 = api.setRangeLock(session, 1, false, false);
            step(3, "User1: Unlock Range 1", r2);

            r2 = api.getRangeInfo(session, 1, info);
            step(4, "Verify unlocked", r2);
            printf("    ReadLocked=%s WriteLocked=%s\n",
                   info.readLocked ? "yes" : "no",
                   info.writeLocked ? "yes" : "no");

            return ErrorCode::Success;
        });

    return r.ok();
}

// ── Scenario 3: SedDrive one-liner ──

static bool scenario3_facade(const char* device, uint16_t comId,
                              cli::CliOptions& opts) {
    scenarioIntent(3, "SedDrive::setupUser() + lockRange/unlockRange",
        { "scenario 1 + 2 의 흐름을 SedDrive facade 한 줄씩 축약.",
          "setupUser 는 enable + password + ACE 를 한 호출로 묶음." },
        { "setupUser(1, USER1_PW, range=1, ADMIN1_PW) OK",
          "lockRange(1, USER1_PW, 1) OK",
          "unlockRange(1, USER1_PW, 1) OK" });

    SedDrive drive(device);
    if (opts.dump) drive.enableDump(std::cerr, opts.dumpLevel);
    drive.query();

    // setupUser bundles: enable + password + ACE assignment
    auto r = drive.setupUser(1, USER1_PW, 1, ADMIN1_PW);
    step(1, "SedDrive::setupUser(1, pw, range=1)", r);
    if (r.failed()) return false;

    // Lock as User1
    r = drive.lockRange(1, USER1_PW, 1);
    step(2, "SedDrive::lockRange(1, User1)", r);

    // Unlock as User1
    r = drive.unlockRange(1, USER1_PW, 1);
    step(3, "SedDrive::unlockRange(1, User1)", r);

    return true;
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
        "User Management — enable users, set passwords, assign ACE");
    if (!transport) return 1;

    SID_PW = getPassword(opts);
    ADMIN1_PW = SID_PW + "_Admin1";
    USER1_PW = SID_PW + "_User1";

    banner("08: User Management");

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    printf("  Setting up drive...\n");
    if (!setupDrive(api, transport, info.baseComId)) {
        printf("  Setup failed.\n"); return 1;
    }

    bool ok = true;
    ok &= scenario1_setupUser(transport, info.baseComId);
    ok &= scenario2_userLockUnlock(transport, info.baseComId);
    ok &= scenario3_facade(opts.device.c_str(), info.baseComId, opts);
    cleanup(transport, info.baseComId);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
