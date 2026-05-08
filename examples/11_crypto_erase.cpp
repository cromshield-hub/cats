/// @file 11_crypto_erase.cpp
/// @brief Crypto Erase — Instant Data Destruction via Key Rotation
///
/// TCG SPEC CONTEXT:
/// Each locking range has an associated AES encryption key (K_AES).
/// ALL data written to the range is encrypted with this key. When you
/// call GenKey on the range, the drive generates a NEW random key —
/// instantly making all previously written data unrecoverable.
///
/// This is "crypto erase" — the physical data remains on the platters/
/// NAND, but without the old key it's indistinguishable from random noise.
/// It completes in milliseconds regardless of drive size.
///
/// GenKey(K_AES_RangeN):
///   - Generates a new random AES key for Range N
///   - The old key is destroyed — data encrypted with it is gone forever
///   - The range configuration (start, length, lock settings) is preserved
///   - The range is automatically unlocked after GenKey
///
/// This is different from "Erase" (secure erase of the physical media)
/// which may take hours. Crypto erase is the standard way to sanitize
/// SED-encrypted data.
///
/// API LAYER: EvalApi + SedDrive
/// PREREQUISITES: 01-07 (need configured locking range)
///
/// Usage: ./11_crypto_erase /dev/nvmeX [--dump]

#include "example_common.h"

static std::string SID_PW;
static std::string ADMIN1_PW;

static bool setupDrive(EvalApi& api, std::shared_ptr<ITransport> transport,
                       uint16_t comId) {
    printf("\n── Setup ───────────────────────────────────────────\n");
    printf("목적:  takeOwnership + Activate(SP_LOCKING) + Admin1 PIN 설정 +\n");
    printf("       Range 1 구성 (start=0, len=1024, lock 활성)\n");
    printf("기대:  scenario 들이 Admin1 권한으로 Range 1 의 K_AES 키를 회전 가능.\n\n");

    printf("[1/3] takeOwnership(SID_PW) — 멱등\n");
    auto cr = composite::takeOwnership(api, transport, comId, SID_PW);
    printf("      → %s\n", cr.ok() ? "OK" : cr.overall.message().c_str());
    if (cr.failed()) return false;

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
    printf("      Lifecycle=0x%02X → %s\n", lifecycle,
           r.ok() ? "OK" : r.message().c_str());
    if (r.failed()) return false;

    printf("[3/3] Admin1 PIN = ADMIN1_PW + Range 1 구성 (4-단계 cred probe)\n");
    Bytes msid;
    composite::getMsid(api, transport, comId, msid);
    auto admin1Setup = [&](const Bytes& cred, const char* label) -> Result {
        printf("      Probe [%-9s] ... ", label); fflush(stdout);
        auto rr = composite::withSession(api, transport, comId,
            uid::SP_LOCKING, true, uid::AUTH_ADMIN1, cred,
            [&](Session& s) -> Result {
                auto r2 = api.setAdmin1Password(s, ADMIN1_PW);
                if (r2.failed()) return r2;
                return api.setRange(s, 1, 0, 1024, true, true);
            });
        printf("%s\n", rr.ok() ? "*** HIT ***" : rr.message().c_str());
        return rr;
    };
    auto r2 = admin1Setup(msid, "MSID");
    if (r2.failed()) r2 = admin1Setup(sidPw, "SID_PW");
    if (r2.failed()) r2 = admin1Setup(pwBytes(ADMIN1_PW), "ADMIN1_PW");
    if (r2.failed()) r2 = admin1Setup(Bytes{}, "empty");
    printf("      결과: %s\n", r2.ok() ? "OK" : "ALL FAILED");
    printf("─────────────────────────────────────────────────────\n\n");
    return r2.ok();
}

// ── Scenario 1: Crypto Erase via GenKey ──
//
// Read the active key UID before and after GenKey to confirm rotation.

static bool scenario1_genKey(std::shared_ptr<ITransport> transport,
                              uint16_t comId) {
    scenarioIntent(1, "Crypto Erase via GenKey",
        { "Range 1 의 K_AES 키를 GenKey 로 회전. UID 는 동일하지만",
          "내부 키 material 이 새로 생성되어 기존 데이터 복호 불가.",
          "(밀리초 단위 instant data destruction)" },
        { "getActiveKey(Range 1) — 회전 전 UID 확인",
          "cryptoErase(Range 1) — GenKey 호출 OK",
          "getActiveKey(Range 1) — 같은 UID (객체 동일, 키만 갱신)",
          "getRangeInfo — Range 구성(start/len/RLE/WLE) 보존됨" });

    EvalApi api;
    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, admin1Pw,
        [&](Session& session) -> Result {
            // Read current active key for Range 1
            Uid keyBefore;
            auto r2 = api.getActiveKey(session, 1, keyBefore);
            step(1, "Get active key (before)", r2);
            if (r2.ok()) {
                printf("    Active Key UID: 0x%016lx\n", keyBefore.toUint64());
            }

            // Crypto Erase!
            // This calls GenKey on the K_AES object for Range 1.
            // Internally: GenKey(K_AES_Range1)
            r2 = api.cryptoErase(session, 1);
            step(2, "cryptoErase(Range 1) — GenKey", r2);
            if (r2.failed()) return r2;

            // Read active key after — it should be the same UID but
            // the actual key material inside has been regenerated.
            // (The UID identifies the key object, not the key value.)
            Uid keyAfter;
            r2 = api.getActiveKey(session, 1, keyAfter);
            step(3, "Get active key (after)", r2);
            if (r2.ok()) {
                printf("    Active Key UID: 0x%016lx\n", keyAfter.toUint64());
                printf("    (Same UID, but internal key material is new)\n");
            }

            // Verify range is still configured
            LockingRangeInfo info;
            r2 = api.getRangeInfo(session, 1, info);
            step(4, "Range 1 still configured after erase", r2);
            if (r2.ok()) {
                printf("    Start=%lu, Length=%lu, RLE=%s, WLE=%s\n",
                       info.rangeStart, info.rangeLength,
                       info.readLockEnabled ? "yes" : "no",
                       info.writeLockEnabled ? "yes" : "no");
            }

            return ErrorCode::Success;
        });

    return r.ok();
}

// ── Scenario 2: Multiple Crypto Erases ──
//
// GenKey can be called repeatedly — each call generates a fresh key.

static bool scenario2_multipleErases(std::shared_ptr<ITransport> transport,
                                      uint16_t comId) {
    scenarioIntent(2, "Multiple Crypto Erases",
        { "GenKey 가 멱등 호출 가능한지 확인. 연속 3 회 호출 시",
          "매번 새 키가 생성되며 누적 에러 없이 처리되어야 함." },
        { "Crypto erase #1 OK",
          "Crypto erase #2 OK",
          "Crypto erase #3 OK — 누적 3 회 키 회전 완료" });

    EvalApi api;
    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, admin1Pw,
        [&](Session& session) -> Result {
            for (int i = 0; i < 3; i++) {
                auto r2 = api.cryptoErase(session, 1);
                char label[64];
                snprintf(label, sizeof(label), "Crypto erase #%d", i + 1);
                step(i + 1, label, r2);
                if (r2.failed()) return r2;
            }
            printf("    All 3 erases completed — key rotated 3 times\n");
            return ErrorCode::Success;
        });

    return r.ok();
}

// ── Scenario 3: SedDrive one-liner ──

static bool scenario3_facade(const char* device, cli::CliOptions& opts) {
    scenarioIntent(3, "SedDrive::cryptoErase()",
        { "scenario 1 의 Admin1 세션 + GenKey 흐름을 facade 한 줄로.",
          "세션 관리/cred 처리 자동화." },
        { "cryptoErase(range=1, ADMIN1_PW) OK" });

    SedDrive drive(device);
    if (opts.dump) drive.enableDump(std::cerr, opts.dumpLevel);
    drive.query();

    auto r = drive.cryptoErase(1, ADMIN1_PW);
    step(1, "SedDrive::cryptoErase(range=1)", r);

    return r.ok();
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
        "Crypto Erase — instant data destruction via key rotation");
    if (!transport) return 1;

    SID_PW = getPassword(opts);
    ADMIN1_PW = SID_PW + "_Admin1";

    banner("11: Crypto Erase");

    if (!confirmDestructive(opts, "crypto-erase locking range keys")) return 0;

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    if (!setupDrive(api, transport, info.baseComId)) {
        printf("  Setup failed.\n"); return 1;
    }

    bool ok = true;
    ok &= scenario1_genKey(transport, info.baseComId);
    ok &= scenario2_multipleErases(transport, info.baseComId);
    ok &= scenario3_facade(opts.device.c_str(), opts);
    cleanup(transport, info.baseComId);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
