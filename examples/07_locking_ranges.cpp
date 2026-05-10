/// @file 07_locking_ranges.cpp
/// @brief Locking Ranges — Configure, Lock, and Unlock
///
/// TCG SPEC CONTEXT:
/// Locking ranges define regions of the disk that can be independently
/// encrypted and locked. Key concepts:
///
///   - Global Range (Range 0): Covers the entire disk by default.
///     UID = 0x0000080200000001
///
///   - Numbered Ranges (Range 1..N): Sub-regions with explicit start LBA
///     and length. Range 1 UID = 0x0000080200000002, etc.
///     Opal 2.0 supports up to 8 ranges (drive-dependent).
///
///   - Each range has independent settings:
///     - RangeStart / RangeLength (LBA boundaries)
///     - ReadLockEnabled / WriteLockEnabled (can it be locked?)
///     - ReadLocked / WriteLocked (is it currently locked?)
///     - LockOnReset (auto-lock on power cycle / StackReset)
///     - ActiveKey (the AES key encrypting this range)
///
///   - Locking/unlocking sets ReadLocked/WriteLocked columns.
///   - Data is always encrypted — "unlocked" means transparent decryption.
///   - "Locked" means reads/writes to those LBAs return errors.
///
/// Authority model:
///   - Admin1 can configure ranges and enable locking.
///   - Users (User1..N) can lock/unlock specific ranges via ACE assignment.
///   - See example 08 for user management.
///
/// API LAYER: EvalApi + SedDrive
/// PREREQUISITES: 01-06 (need Locking SP activated)
///
/// Usage: ./07_locking_ranges /dev/nvmeX [--dump]

#include "example_common.h"

static std::string SID_PW;
static std::string ADMIN1_PW;

// Helper: setup drive (own + activate + set Admin1 pw)
static bool setupDrive(EvalApi& api, std::shared_ptr<ITransport> transport,
                       uint16_t comId) {
    auto cr = composite::takeOwnership(api, transport, comId, SID_PW);
    if (cr.failed()) { printf("  takeOwnership failed\n"); return false; }

    // Activate Locking SP
    Bytes sidPw = pwBytes(SID_PW);
    auto r = composite::withSession(api, transport, comId,
        uid::SP_ADMIN, true, uid::AUTH_SID, sidPw,
        [&](Session& s) { return api.activate(s, uid::SP_LOCKING); });
    if (r.failed()) { printf("  activate failed\n"); return false; }

    // Set Admin1 password
    r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, Bytes{},
        [&](Session& s) { return api.setAdmin1Password(s, ADMIN1_PW); });
    if (r.failed()) { printf("  setAdmin1Password failed\n"); return false; }

    return true;
}

// ── Scenario 1: Configure and Lock/Unlock a Range ──
//
// Set up Range 1 with a specific LBA region, enable locking,
// then lock it, verify it's locked, and unlock.

static bool scenario1_rangeLifecycle(std::shared_ptr<ITransport> transport,
                                      uint16_t comId) {
    scenarioIntent(1, "Range 1: Configure → Lock → Unlock",
        { "Range 1 을 LBA 0..1023 으로 설정하고 RLE/WLE 켠 뒤,",
          "lock → 상태확인 → unlock → 상태확인 의 한 사이클." },
        { "setRange (range/length/RLE/WLE) 성공",
          "getRangeInfo 로 설정값 readback OK",
          "setRangeLock(true,true) → readLocked/writeLocked = yes",
          "setRangeLock(false,false) → 다시 unlocked 확인" });

    EvalApi api;
    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, admin1Pw,
        [&](Session& session) -> Result {

            // Configure Range 1: LBA 0..1023, enable read+write locking
            auto r2 = api.setRange(session, 1,
                0,      // rangeStart (LBA)
                1024,   // rangeLength (sectors)
                true,   // readLockEnabled
                true);  // writeLockEnabled
            step(1, "Configure Range 1 (LBA 0-1023)", r2);
            if (r2.failed()) return r2;

            // Read back the range configuration
            LockingRangeInfo info;
            r2 = api.getRangeInfo(session, 1, info);
            step(2, "Read Range 1 info", r2);
            if (r2.ok()) {
                printf("    Start: %lu, Length: %lu\n", info.rangeStart, info.rangeLength);
                printf("    RLE=%s, WLE=%s\n",
                       info.readLockEnabled ? "yes" : "no",
                       info.writeLockEnabled ? "yes" : "no");
                printf("    Locked: read=%s, write=%s\n",
                       info.readLocked ? "yes" : "no",
                       info.writeLocked ? "yes" : "no");
            }

            // Lock Range 1 (set ReadLocked=true, WriteLocked=true)
            r2 = api.setRangeLock(session, 1, true, true);
            step(3, "Lock Range 1", r2);

            // Verify locked state
            r2 = api.getRangeInfo(session, 1, info);
            step(4, "Verify Range 1 locked", r2);
            if (r2.ok()) {
                printf("    Locked: read=%s, write=%s\n",
                       info.readLocked ? "yes" : "no",
                       info.writeLocked ? "yes" : "no");
            }

            // Unlock Range 1
            r2 = api.setRangeLock(session, 1, false, false);
            step(5, "Unlock Range 1", r2);

            // Verify unlocked
            r2 = api.getRangeInfo(session, 1, info);
            step(6, "Verify Range 1 unlocked", r2);
            if (r2.ok()) {
                printf("    Locked: read=%s, write=%s\n",
                       info.readLocked ? "yes" : "no",
                       info.writeLocked ? "yes" : "no");
            }

            return ErrorCode::Success;
        });

    step(7, "Range lifecycle complete", r);
    return r.ok();
}

// ── Scenario 2: Global Range (Range 0) ──
//
// The Global Range covers all LBAs not assigned to numbered ranges.
// You can enable locking on it, but be careful — locking Range 0
// locks the entire disk (minus explicitly assigned ranges).

static bool scenario2_globalRange(std::shared_ptr<ITransport> transport,
                                   uint16_t comId) {
    scenarioIntent(2, "Global Range (Range 0) Inspection",
        { "Range 0 (Global Range) 의 현재 설정 + 잠금 상태 read.",
          "Global Range 를 lock 하면 명시 할당된 range 외 전체 디스크가 잠긴다 — 주의." },
        { "getRangeInfo(0) 성공",
          "Global Range 의 start/length, RLE/WLE, lock 상태 출력" });

    EvalApi api;
    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, admin1Pw,
        [&](Session& session) -> Result {
            LockingRangeInfo info;
            auto r2 = api.getRangeInfo(session, 0, info);
            step(1, "Read Global Range info", r2);
            if (r2.ok()) {
                printf("    Start: %lu, Length: %lu\n", info.rangeStart, info.rangeLength);
                printf("    RLE=%s, WLE=%s\n",
                       info.readLockEnabled ? "yes" : "no",
                       info.writeLockEnabled ? "yes" : "no");
                printf("    Locked: read=%s, write=%s\n",
                       info.readLocked ? "yes" : "no",
                       info.writeLocked ? "yes" : "no");
            }
            return r2;
        });

    return r.ok();
}

// ── Scenario 3: Enumerate all ranges ──

static bool scenario3_enumerateRanges(std::shared_ptr<ITransport> transport,
                                       uint16_t comId) {
    scenarioIntent(3, "Enumerate All Locking Ranges",
        { "getAllLockingInfo 로 한 호출에 모든 range 정보 수집.",
          "Opal 표준 9 개 range 까지 일괄 표시." },
        { "getAllLockingInfo(max=9) 성공",
          "Range 0..N 에 대해 start/len/RLE/WLE/RL/WL 행 단위 출력" });

    EvalApi api;
    Bytes admin1Pw = pwBytes(ADMIN1_PW);

    auto r = composite::withSession(api, transport, comId,
        uid::SP_LOCKING, true, uid::AUTH_ADMIN1, admin1Pw,
        [&](Session& session) -> Result {
            std::vector<LockingInfo> ranges;
            auto r2 = api.getAllLockingInfo(session, ranges, 9);
            step(1, "getAllLockingInfo(max=9)", r2);

            printf("    Found %zu ranges:\n", ranges.size());
            for (auto& ri : ranges) {
                printf("    Range %u: start=%lu len=%lu RLE=%d WLE=%d RL=%d WL=%d\n",
                       ri.rangeId, ri.rangeStart, ri.rangeLength,
                       ri.readLockEnabled, ri.writeLockEnabled,
                       ri.readLocked, ri.writeLocked);
            }
            return r2;
        });

    return r.ok();
}

// ── Cleanup ──

static bool cleanup(std::shared_ptr<ITransport> transport, uint16_t comId) {
    scenarioIntent(0, "Cleanup — Revert to Factory",
        { "위 시나리오들이 만든 잠금/range 상태를 모두 되돌림.",
          "RevertToFactory — SID 비번을 알고 있으므로 전체 데이터 영향 없음." },
        { "composite::revertToFactory 성공" });
    EvalApi api;
    auto cr = composite::revertToFactory(api, transport, comId, SID_PW);
    step(1, "RevertToFactory", cr.overall);
    return cr.ok();
}

int main(int argc, char* argv[]) {
    cli::CliOptions opts;
    auto transport = initTransport(argc, argv, opts,
        "Locking Ranges — configure, lock, and unlock disk regions");
    if (!transport) return 1;

    SID_PW = getPassword(opts);
    ADMIN1_PW = SID_PW + "_Admin1";

    banner("07: Locking Ranges");

    EvalApi api;
    DiscoveryInfo info;
    auto r = api.discovery0(transport, info);
    if (r.failed()) { printf("Discovery failed\n"); return 1; }

    // Setup: own + activate
    printf("  Setting up drive (ownership + activation)...\n");
    if (!setupDrive(api, transport, info.baseComId)) {
        printf("  Setup failed. Drive must be in factory state.\n");
        return 1;
    }

    bool ok = true;
    ok &= scenario1_rangeLifecycle(transport, info.baseComId);
    ok &= scenario2_globalRange(transport, info.baseComId);
    ok &= scenario3_enumerateRanges(transport, info.baseComId);
    cleanup(transport, info.baseComId);

    printf("\n%s\n", ok ? "All scenarios passed." : "Some scenarios failed.");
    return ok ? 0 : 1;
}
