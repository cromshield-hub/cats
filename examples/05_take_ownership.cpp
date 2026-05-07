/// @file 05_take_ownership.cpp
/// @brief Take Ownership — Change SID Password from MSID
///
/// TCG SPEC CONTEXT:
/// "Taking ownership" means changing the SID password from the factory
/// default (MSID) to a secret only you know. This is the most important
/// step in securing a drive — until you do this, anyone who can read
/// the MSID (printed on the drive label) has full control.
///
/// Protocol flow (AppNote Section 3):
///   1. Anonymous session → Admin SP → read C_PIN_MSID.PIN
///   2. Authenticated write session → Admin SP → SID auth with MSID
///   3. Set(C_PIN_SID, column PIN, newPassword)
///   4. Close session
///
/// After this, the SID password is your chosen secret. The MSID doesn't
/// change — it's burned into the drive — but it no longer grants access.
///
/// IMPORTANT: Remember your SID password! If you lose it and don't have
/// the PSID (Physical Security ID, also on the label), the only way to
/// recover is PSID Revert, which destroys all data and keys.
///
/// This example reverts to factory state at the end to leave the drive clean.
///
/// API LAYER: Both EvalApi (step-by-step) and SedDrive (one-liner).
/// PREREQUISITES: 01-04 (Discovery, Properties, Sessions, MSID)
///
/// Usage: ./05_take_ownership /dev/nvmeX [--dump]

#include "example_common.h"

static std::string TEST_SID_PW;

// ── Scenario 1: Take Ownership step-by-step (EvalApi) ──
//
// The full protocol flow with explicit session management.
// This is what happens "under the hood" when you call SedDrive::takeOwnership().

static bool scenario1_evalOwnership(std::shared_ptr<ITransport> transport,
                                     uint16_t comId,
                                     const PropertiesResult& props) {
    scenario(1, "Take Ownership Step-by-Step (EvalApi)");
    printf("  Intent: factory-state 드라이브에서 SID 비번을 MSID -> TEST_SID_PW 로\n");
    printf("          교체하고, 새 비번 동작 + 옛 MSID 무효화를 모두 검증.\n\n");

    EvalApi api;

    // ── Step 1: Anonymous read session to AdminSP ──
    // Intent: factory state 에서 익명으로 진입 가능해야 함. (성공이 정답)
    Session anonSession(transport, comId);
    anonSession.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr;
    auto r = api.startSession(anonSession, uid::SP_ADMIN, false, ssr);
    stepExpect(1, "Anonymous session to AdminSP", Expect::Success, r);
    if (r.failed()) return false;

    // ── Step 2: Read C_PIN_MSID ──
    // Intent: 익명 세션에서도 MSID 읽기는 허용됨 (factory secret 노출). (성공이 정답)
    Bytes msid;
    r = api.getCPin(anonSession, uid::CPIN_MSID, msid);
    stepExpect(2, "Read C_PIN_MSID", Expect::Success, r);
    api.closeSession(anonSession);
    if (r.failed() || msid.empty()) {
        printf("    Cannot read MSID\n");
        return false;
    }
    printString("MSID", msid);

    // ── Step 3: Authenticate as SID using MSID ──
    // Intent: 드라이브가 factory state 라면 C_PIN_SID == MSID 이므로 성공.
    //         이미 owned 라면 NotAuthorized — 이 경우 take_own 진행 불가하므로
    //         scenario 를 skip 으로 종료 (test failure 가 아님, 정보성).
    Session authSession(transport, comId);
    authSession.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr2;
    r = api.startSessionWithAuth(authSession, uid::SP_ADMIN, true,
                                  uid::AUTH_SID, msid, ssr2);
    stepExpect(3, "SID-auth write session (cred=MSID)", Expect::Success, r);
    if (r.failed()) {
        printf("    Note: 드라이브가 이미 owned 상태로 보임. take_own 단계 skip.\n");
        printf("          공장 상태로 복원하려면 example 12 (factory_reset) 먼저 실행.\n");
        return true;  // 정보성 종료 — scenario 자체는 fail 아님
    }

    // ── Step 4: Set new SID password ──
    // Intent: C_PIN_SID 를 hash(TEST_SID_PW) 로 덮어씀. (성공이 정답)
    // wire 상 페이로드 = HashPassword::passwordToBytes(TEST_SID_PW) 의 32B.
    r = api.setCPin(authSession, uid::CPIN_SID, TEST_SID_PW);
    stepExpect(4, "Set C_PIN_SID to new password", Expect::Success, r);
    api.closeSession(authSession);
    if (r.failed()) return false;
    printf("    SID password changed to: \"%s\"\n", TEST_SID_PW.c_str());

    // ── Step 5: Verify new password works ──
    // Intent: 방금 박은 비번으로 SID 인증 성공해야 함. (성공이 정답)
    // wire challenge = step 4 의 SetCPin 페이로드와 byte-identical.
    Session verifySession(transport, comId);
    verifySession.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr3;
    Bytes sidPin = pwBytes(TEST_SID_PW);
    r = api.startSessionWithAuth(verifySession, uid::SP_ADMIN, true,
                                  uid::AUTH_SID, sidPin, ssr3);
    stepExpect(5, "Verify SID auth with NEW password", Expect::Success, r);
    api.closeSession(verifySession);
    if (r.failed()) return false;

    // ── Step 6: Verify old MSID no longer authenticates ──
    // Intent: take_own 이 정말 효과를 냈다면 옛 MSID 는 이제 거부돼야 함.
    //         **NotAuthorized 가 정답** (negative test). 의도적으로 MSID 를
    //         그대로 보내고, 거절 받으면 PASS.
    Session failSession(transport, comId);
    failSession.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr4;
    r = api.startSessionWithAuth(failSession, uid::SP_ADMIN, true,
                                  uid::AUTH_SID, msid, ssr4);
    stepExpect(6, "Verify OLD MSID is now rejected", Expect::Failure, r);
    bool intentMet = r.failed();
    if (!intentMet) api.closeSession(failSession);

    return intentMet;
}

// ── Scenario 2: Revert to factory (restore MSID as SID) ──
//
// Important: always clean up after ownership tests!
// AdminSP.Revert() (UID 0x0202) resets SID password back to MSID.
// SID 권한으로 호출 가능 (sedutil --revertTPer 와 동일 wire).
// AdminSP.RevertSP() (0x0011) 은 PSID 권한이 필요하므로 SID 세션에서 NotAuthorized.

static bool scenario2_revert(std::shared_ptr<ITransport> transport,
                              uint16_t comId,
                              const PropertiesResult& props) {
    scenario(2, "Revert to Factory State");
    printf("  Intent: scenario 1 에서 박은 새 SID 비번을 사용해 AdminSP.Revert()\n");
    printf("          호출. 드라이브를 factory state 로 되돌리고 MSID 가 다시\n");
    printf("          credential 로 작동하는지 검증.\n\n");

    EvalApi api;

    // ── Step 1: SID auth with current (= new) password ──
    // Intent: scenario 1 step 4 에서 박은 비번으로 SID 인증. (성공이 정답)
    Session session(transport, comId);
    session.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr;
    Bytes sidPin = pwBytes(TEST_SID_PW);
    auto r = api.startSessionWithAuth(session, uid::SP_ADMIN, true,
                                       uid::AUTH_SID, sidPin, ssr);
    stepExpect(1, "SID auth with CURRENT password", Expect::Success, r);
    if (r.failed()) return false;

    // ── Step 2: AdminSP.Revert() ──
    // Intent: AdminSP 를 factory state 로 복원. SID 권한으로 호출 가능
    //         (sedutil --revertTPer 와 동일). (성공이 정답)
    // 주의: api.revertSP (UID 0x0011) 가 아니라 api.revert (UID 0x0202).
    //       전자는 PSID 권한 필요.
    RawResult raw;
    r = api.revert(session, uid::SP_ADMIN, raw);
    stepExpect(2, "AdminSP.Revert() (cred=SID)", Expect::Success, r);
    if (r.failed()) {
        printf("    Method status: 0x%02X (%s)\n",
               static_cast<unsigned>(raw.methodResult.status()),
               raw.methodResult.statusMessage().c_str());
        return false;
    }
    // Session is invalidated by TPer after Revert, no need to close.

    // ── Step 3: Verify MSID is now usable again ──
    // Intent: revert 가 효과를 냈다면 SID == MSID 로 돌아갔어야 함. 익명으로
    //         MSID 를 다시 읽고, 그걸 credential 로 SID 인증이 성공해야 함.
    //         (성공이 정답)
    Session verifySession(transport, comId);
    verifySession.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr2;
    auto r2 = api.startSession(verifySession, uid::SP_ADMIN, false, ssr2);
    Bytes msid;
    if (r2.ok()) {
        api.getCPin(verifySession, uid::CPIN_MSID, msid);
        api.closeSession(verifySession);
    }

    if (msid.empty()) {
        step(3, "Verify: MSID readable after revert", false);
        return false;
    }

    Session msidAuth(transport, comId);
    msidAuth.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr3;
    r2 = api.startSessionWithAuth(msidAuth, uid::SP_ADMIN, true,
                                   uid::AUTH_SID, msid, ssr3);
    stepExpect(3, "Verify SID auth with MSID (post-revert)",
               Expect::Success, r2);
    if (r2.ok()) api.closeSession(msidAuth);

    return r2.ok();
}

// ── Scenario 3: SedDrive one-liner ──
//
// SedDrive::takeOwnership() does everything in one call.
// Then SedDrive::revert() cleans up.

static bool scenario3_facade(const char* device, cli::CliOptions& opts) {
    scenario(3, "SedDrive One-Liner Ownership");
    printf("  Intent: scenario 1 의 step-by-step 흐름을 SedDrive facade 한 줄로\n");
    printf("          축약. composite::takeOwnership 으로 라우팅되므로 멱등성과\n");
    printf("          SpBusy 자동복구를 함께 활용 (commit 5f153a9 / f580e7c).\n\n");

    SedDrive drive(device);
    if (opts.dump) drive.enableDump(std::cerr, opts.dumpLevel);
    auto r = drive.query();
    if (r.failed()) return false;

    // ── Step 1: takeOwnership ──
    // Intent: 새 비번을 SID 로 박음. 드라이브 상태에 따라:
    //         - factory state: 정상 take_own → Success
    //         - 이미 같은 비번으로 owned: 멱등 no-op → Success
    //         - 다른 비번으로 owned: AlreadyOwnedDifferentCredential
    //         성공 또는 멱등 케이스가 정답.
    r = drive.takeOwnership(TEST_SID_PW);
    stepExpect(1, "SedDrive::takeOwnership()", Expect::Success, r);
    if (r.failed()) return false;

    // ── Step 2: revert ──
    // Intent: 같은 facade 의 revert 로 원복. 내부적으로 AdminSP.Revert()
    //         (UID 0x0202) 를 SID 권한으로 호출 (commit 2b7d67e). (성공이 정답)
    r = drive.revert(TEST_SID_PW);
    stepExpect(2, "SedDrive::revert() (back to factory)", Expect::Success, r);

    return r.ok();
}

int main(int argc, char* argv[]) {
    cli::CliOptions opts;
    auto transport = initTransport(argc, argv, opts,
        "Take Ownership — change SID password from MSID");
    if (!transport) return 1;

    TEST_SID_PW = getPassword(opts);

    banner("05: Take Ownership");
    printf("  WARNING: This example changes the SID password and reverts.\n");
    printf("  The drive should be in factory state (SID == MSID).\n\n");

    if (!confirmDestructive(opts, "change the SID password")) return 0;

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    // Properties must be exchanged before any session to match the sedutil wire
    // pattern — some drives return NOT_AUTHORIZED / TPER_MALFUNCTION on auth
    // attempts made before Properties. Internally this also runs StackReset
    // to force the ComID to Issued(idle) state.
    PropertiesResult props;
    r = api.exchangeProperties(transport, info.baseComId, props);
    if (r.failed()) { printf("Properties exchange failed\n"); return 1; }

    bool ok = true;
    ok &= scenario1_evalOwnership(transport, info.baseComId, props);
    ok &= scenario2_revert(transport, info.baseComId, props);
    ok &= scenario3_facade(opts.device.c_str(), opts);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
