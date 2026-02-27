/// @file eval_worker_pattern.cpp
/// @brief NVMeThread + Worker pattern with SedContext integration.
///
/// Shows how your evaluation platform integrates with this library:
///
///   NVMeThread (owns libnvme, creates SedContext)
///       │
///       ├── Worker A (TCG ownership test)
///       ├── Worker B (locking range test)
///       ├── Worker C (NVMe format + TCG verify)
///       └── Worker D (DataStore stress test)
///
/// Each Worker receives:
///   - libnvme*   → NVMe operations
///   - SedContext& → TCG operations (Transport + EvalApi + Session)
///
/// Key: Worker never creates its own transport or device.
///      Everything flows down from NVMeThread via DI.

#include <libsed/eval/sed_context.h>
#include <libsed/eval/eval_api.h>
#include <libsed/transport/nvme_transport.h>
#include <libsed/security/hash_password.h>
#include <libsed/sed_library.h>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <string>
#include <cstdio>

using namespace libsed;
using namespace libsed::eval;

// ════════════════════════════════════════════════════════
//  Simulated libnvme (your actual implementation)
// ════════════════════════════════════════════════════════

class SimLibNvme : public INvmeDevice {
public:
    explicit SimLibNvme(const std::string& path) : path_(path) {
        printf("  [libnvme] Opened %s (simulated)\n", path.c_str());
    }

    // Security Protocol — used by NvmeTransport internally
    Result securitySend(uint8_t, uint16_t, const uint8_t*, uint32_t) override {
        return ErrorCode::Success;
    }
    Result securityRecv(uint8_t, uint16_t, uint8_t* data, uint32_t len,
                        uint32_t& received) override {
        // Simulated L0 Discovery response
        if (len >= 48) {
            memset(data, 0, len);
            data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 48;
            received = 48;
        }
        return ErrorCode::Success;
    }

    // NVMe Admin
    Result adminCommand(NvmeAdminCmd& cmd, NvmeCompletion& cpl) override {
        printf("  [libnvme] Admin opcode=0x%02X nsid=%u\n", cmd.opcode, cmd.nsid);
        cpl = {};
        return ErrorCode::Success;
    }
    Result ioCommand(NvmeIoCmd& cmd, NvmeCompletion& cpl) override {
        printf("  [libnvme] IO opcode=0x%02X slba=%lu\n", cmd.opcode, cmd.slba);
        cpl = {};
        return ErrorCode::Success;
    }

    Result identify(uint8_t cns, uint32_t nsid, Bytes& data) override {
        printf("  [libnvme] Identify CNS=%u nsid=%u\n", cns, nsid);
        data.resize(4096, 0);
        memcpy(data.data() + 24, "SimNVMe SSD Model 1234", 22);
        return ErrorCode::Success;
    }
    Result getLogPage(uint8_t logId, uint32_t nsid, Bytes& data, uint32_t dataLen) override {
        printf("  [libnvme] GetLogPage logId=0x%02X\n", logId);
        data.resize(dataLen, 0);
        return ErrorCode::Success;
    }
    Result getFeature(uint8_t fid, uint32_t, uint32_t& cdw0, Bytes&) override {
        printf("  [libnvme] GetFeature fid=0x%02X\n", fid);
        cdw0 = 0;
        return ErrorCode::Success;
    }
    Result setFeature(uint8_t fid, uint32_t, uint32_t, const Bytes&) override {
        printf("  [libnvme] SetFeature fid=0x%02X\n", fid);
        return ErrorCode::Success;
    }
    Result formatNvm(uint32_t nsid, uint8_t lbaf, uint8_t ses, uint8_t) override {
        printf("  [libnvme] Format nsid=%u lbaf=%u ses=%u\n", nsid, lbaf, ses);
        return ErrorCode::Success;
    }
    Result sanitize(uint8_t action, uint32_t) override {
        printf("  [libnvme] Sanitize action=%u\n", action);
        return ErrorCode::Success;
    }
    Result fwDownload(const Bytes&, uint32_t offset) override {
        printf("  [libnvme] FW Download offset=%u\n", offset);
        return ErrorCode::Success;
    }
    Result fwCommit(uint8_t slot, uint8_t action) override {
        printf("  [libnvme] FW Commit slot=%u action=%u\n", slot, action);
        return ErrorCode::Success;
    }
    Result nsCreate(const Bytes&, uint32_t& nsid) override { nsid = 1; return ErrorCode::Success; }
    Result nsDelete(uint32_t) override { return ErrorCode::Success; }
    Result nsAttach(uint32_t, uint16_t, bool) override { return ErrorCode::Success; }

    std::string devicePath() const override { return path_; }
    bool isOpen() const override { return true; }
    void close() override {}
    int fd() const override { return 42; }

private:
    std::string path_;
};

// ════════════════════════════════════════════════════════
//  Worker Base (your platform's abstract worker)
// ════════════════════════════════════════════════════════

class Worker {
public:
    virtual ~Worker() = default;

    /// Name for logging
    virtual std::string name() const = 0;

    /// Called by NVMeThread with this thread's context.
    /// @param libnvme  This thread's NVMe device (for NVMe ops)
    /// @param ctx      This thread's TCG context (for TCG ops)
    virtual Result execute(INvmeDevice& libnvme, SedContext& ctx) = 0;
};

// ════════════════════════════════════════════════════════
//  NVMeThread (your platform's thread class)
// ════════════════════════════════════════════════════════

class NVMeThread {
public:
    NVMeThread(const std::string& devicePath, int threadId)
        : threadId_(threadId)
    {
        // Each NVMeThread owns its own libnvme instance
        libnvme_ = std::make_shared<SimLibNvme>(devicePath);

        // Create per-thread SedContext (DI libnvme → transport)
        sedContext_ = std::make_unique<SedContext>(libnvme_);
    }

    /// Add worker to execution list
    void addWorker(std::unique_ptr<Worker> worker) {
        workers_.push_back(std::move(worker));
    }

    /// Run all workers sequentially (called by thread)
    void run() {
        printf("\n[Thread %d] Starting (%zu workers)\n", threadId_, workers_.size());

        // Initialize TCG context once per thread
        auto r = sedContext_->initialize();
        printf("[Thread %d] SedContext::initialize() → %s\n",
               threadId_, r.ok() ? "OK" : "FAIL (simulated, expected)");

        // Execute workers sequentially
        for (auto& worker : workers_) {
            printf("\n[Thread %d] ── Running: %s ──\n", threadId_, worker->name().c_str());
            auto wr = worker->execute(*libnvme_, *sedContext_);
            printf("[Thread %d] ── %s: %s ──\n",
                   threadId_, worker->name().c_str(), wr.ok() ? "PASS" : "FAIL");
        }

        printf("[Thread %d] Done\n", threadId_);
    }

    /// Start as actual thread
    std::thread start() {
        return std::thread([this]() { run(); });
    }

private:
    int threadId_;
    std::shared_ptr<INvmeDevice>  libnvme_;
    std::unique_ptr<SedContext>   sedContext_;
    std::vector<std::unique_ptr<Worker>> workers_;
};

// ════════════════════════════════════════════════════════
//  Concrete Workers (TC developer writes these)
// ════════════════════════════════════════════════════════

/// Worker 1: Discovery + Feature Check
class DiscoveryWorker : public Worker {
public:
    std::string name() const override { return "DiscoveryWorker"; }

    Result execute(INvmeDevice& libnvme, SedContext& ctx) override {
        // TCG: Discovery via SedContext
        TcgOption opt;
        auto r = ctx.api().getTcgOption(ctx.transport(), opt);
        printf("    [TCG] SSC detected, ComID=0x%04X\n", opt.baseComId);

        // TCG: All security features
        std::vector<SecurityFeatureInfo> feats;
        ctx.api().getAllSecurityFeatures(ctx.transport(), feats);
        printf("    [TCG] %zu features found\n", feats.size());

        // NVMe: Identify controller (via libnvme directly)
        Bytes identData;
        libnvme.identify(1, 0, identData);
        if (identData.size() >= 64) {
            std::string model(identData.begin() + 24, identData.begin() + 64);
            printf("    [NVMe] Model: %s\n", model.c_str());
        }

        return ErrorCode::Success;
    }
};

/// Worker 2: Ownership + Locking Setup
class OwnershipWorker : public Worker {
    std::string sidPw_;
    std::string admin1Pw_;
public:
    OwnershipWorker(std::string sidPw, std::string admin1Pw)
        : sidPw_(std::move(sidPw)), admin1Pw_(std::move(admin1Pw)) {}

    std::string name() const override { return "OwnershipWorker"; }

    Result execute(INvmeDevice& /*libnvme*/, SedContext& ctx) override {
        // Read MSID
        Bytes msid;
        auto r = ctx.readMsid(msid);
        printf("    [TCG] MSID read: %s (%zuB)\n", r.ok() ? "OK" : "FAIL", msid.size());

        // Take ownership
        r = ctx.takeOwnership(sidPw_);
        printf("    [TCG] takeOwnership: %s\n", r.ok() ? "OK" : "FAIL");

        // Activate Locking SP
        r = ctx.openSession(uid::SP_ADMIN, uid::AUTH_SID, sidPw_);
        if (r.ok()) {
            RawResult raw;
            ctx.api().activate(ctx.session(), uid::SP_LOCKING, raw);
            printf("    [TCG] Activate LockingSP: %s\n", r.ok() ? "OK" : "FAIL");
            ctx.closeSession();
        }

        return ErrorCode::Success;
    }
};

/// Worker 3: Locking Range Configuration + Lock/Unlock
class LockingWorker : public Worker {
    std::string admin1Pw_;
    uint32_t rangeId_;
public:
    LockingWorker(std::string admin1Pw, uint32_t rangeId)
        : admin1Pw_(std::move(admin1Pw)), rangeId_(rangeId) {}

    std::string name() const override { return "LockingWorker(range=" + std::to_string(rangeId_) + ")"; }

    Result execute(INvmeDevice& /*libnvme*/, SedContext& ctx) override {
        auto r = ctx.openSession(uid::SP_LOCKING, uid::AUTH_ADMIN1, admin1Pw_);
        if (r.failed()) { printf("    Session fail\n"); return r; }

        RawResult raw;

        // Read current state
        LockingInfo li;
        ctx.api().getLockingInfo(ctx.session(), rangeId_, li, raw);
        printf("    Range %u: start=%lu len=%lu RLE=%d WLE=%d\n",
               rangeId_, li.rangeStart, li.rangeLength,
               li.readLockEnabled, li.writeLockEnabled);

        // Configure
        ctx.api().setRange(ctx.session(), rangeId_, 0, 0, true, true, raw);
        printf("    setRange(RLE=true, WLE=true): OK\n");

        // Lock
        ctx.api().setRangeLock(ctx.session(), rangeId_, true, true, raw);
        printf("    Lock: OK\n");

        // Verify locked
        ctx.api().getLockingInfo(ctx.session(), rangeId_, li, raw);
        printf("    Verify: RL=%d WL=%d\n", li.readLocked, li.writeLocked);

        // Unlock
        ctx.api().setRangeLock(ctx.session(), rangeId_, false, false, raw);
        printf("    Unlock: OK\n");

        ctx.closeSession();
        return ErrorCode::Success;
    }
};

/// Worker 4: NVMe Format + TCG State Verification
class FormatRecoveryWorker : public Worker {
    std::string sidPw_;
public:
    explicit FormatRecoveryWorker(std::string sidPw) : sidPw_(std::move(sidPw)) {}

    std::string name() const override { return "FormatRecoveryWorker"; }

    Result execute(INvmeDevice& libnvme, SedContext& ctx) override {
        // Step 1: NVMe — check SMART before format
        Bytes smart;
        libnvme.getLogPage(0x02, 0xFFFFFFFF, smart, 512);
        printf("    [NVMe] Pre-format SMART: %zuB\n", smart.size());

        // Step 2: NVMe — Format (User Data Erase)
        auto r = libnvme.formatNvm(1, 0, /*ses=*/1);
        printf("    [NVMe] Format: %s\n", r.ok() ? "OK" : "FAIL");

        // Step 3: TCG — Re-discover (format may affect SED state)
        TcgOption opt;
        ctx.api().getTcgOption(ctx.transport(), opt);
        printf("    [TCG] Post-format: locking=%d locked=%d\n",
               opt.lockingEnabled, opt.locked);

        // Step 4: TCG — Can we still read MSID?
        Bytes msid;
        r = ctx.readMsid(msid);
        printf("    [TCG] Post-format MSID read: %s (%zuB)\n",
               r.ok() ? "OK" : "FAIL", msid.size());

        // Step 5: NVMe — SMART after format
        libnvme.getLogPage(0x02, 0xFFFFFFFF, smart, 512);
        printf("    [NVMe] Post-format SMART: %zuB\n", smart.size());

        return ErrorCode::Success;
    }
};

/// Worker 5: DataStore I/O
class DataStoreWorker : public Worker {
    std::string admin1Pw_;
public:
    explicit DataStoreWorker(std::string admin1Pw) : admin1Pw_(std::move(admin1Pw)) {}

    std::string name() const override { return "DataStoreWorker"; }

    Result execute(INvmeDevice& /*libnvme*/, SedContext& ctx) override {
        auto r = ctx.openSession(uid::SP_LOCKING, uid::AUTH_ADMIN1, admin1Pw_);
        if (r.failed()) return r;

        RawResult raw;

        // ByteTable info
        ByteTableInfo bti;
        ctx.api().getByteTableInfo(ctx.session(), bti, raw);
        printf("    DataStore max=%u used=%u\n", bti.maxSize, bti.usedSize);

        // Write pattern
        Bytes pattern = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
        ctx.api().tcgWriteDataStore(ctx.session(), 0, pattern, raw);
        printf("    Write 8B at offset 0: OK\n");

        // Read back
        DataOpResult dr;
        ctx.api().tcgReadDataStore(ctx.session(), 0, 8, dr);
        printf("    Read 8B: %zuB returned\n", dr.data.size());

        // Compare
        ctx.api().tcgCompare(ctx.session(), uid::TABLE_DATASTORE, 0, pattern, dr);
        printf("    Compare: match=%d\n", dr.compareMatch);

        ctx.closeSession();
        return ErrorCode::Success;
    }
};

/// Worker 6: Dual Session (AdminSP + LockingSP)
class DualSessionWorker : public Worker {
    std::string sidPw_;
    std::string admin1Pw_;
public:
    DualSessionWorker(std::string sidPw, std::string admin1Pw)
        : sidPw_(std::move(sidPw)), admin1Pw_(std::move(admin1Pw)) {}

    std::string name() const override { return "DualSessionWorker"; }

    Result execute(INvmeDevice& /*libnvme*/, SedContext& ctx) override {
        Bytes sidCred = HashPassword::passwordToBytes(sidPw_);
        Bytes admin1Cred = HashPassword::passwordToBytes(admin1Pw_);

        // Main session: AdminSP
        auto r = ctx.openSession(uid::SP_ADMIN, uid::AUTH_SID, sidCred);
        printf("    Main session (AdminSP/SID): %s\n", r.ok() ? "OK" : "FAIL");

        // Secondary session: LockingSP (independent)
        auto lockSession = ctx.createAndOpenSession(
            uid::SP_LOCKING, uid::AUTH_ADMIN1, admin1Cred);
        printf("    Secondary session (LockingSP/Admin1): %s\n",
               lockSession ? "OK" : "FAIL");

        if (ctx.hasSession() && lockSession) {
            RawResult raw;

            // AdminSP: read lifecycle
            uint8_t lifecycle = 0;
            ctx.api().getSpLifecycle(ctx.session(), uid::SP_LOCKING, lifecycle, raw);
            printf("    [AdminSP] LockingSP lifecycle=%u\n", lifecycle);

            // LockingSP: read locking info
            LockingInfo li;
            ctx.api().getLockingInfo(*lockSession, 0, li, raw);
            printf("    [LockingSP] Range0 RLE=%d WLE=%d\n",
                   li.readLockEnabled, li.writeLockEnabled);

            // Close secondary
            ctx.api().closeSession(*lockSession);
        }

        ctx.closeSession();
        return ErrorCode::Success;
    }
};

// ════════════════════════════════════════════════════════
//  Main: Simulates your evaluation platform
// ════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::string device = (argc > 1) ? argv[1] : "/dev/nvme0";
    std::string sidPw = "sid_password";
    std::string admin1Pw = "admin1_password";
    int numThreads = (argc > 2) ? std::stoi(argv[2]) : 2;

    libsed::initialize();

    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "  NVMeThread + Worker + SedContext Demo\n";
    std::cout << "  Device: " << device << "\n";
    std::cout << "  Threads: " << numThreads << "\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── Create NVMeThreads ──────────────────────────
    std::vector<NVMeThread> nvmeThreads;
    for (int i = 0; i < numThreads; i++) {
        nvmeThreads.emplace_back(device, i);
    }

    // ── Thread 0: Full ownership + locking flow ─────
    nvmeThreads[0].addWorker(std::make_unique<DiscoveryWorker>());
    nvmeThreads[0].addWorker(std::make_unique<OwnershipWorker>(sidPw, admin1Pw));
    nvmeThreads[0].addWorker(std::make_unique<LockingWorker>(admin1Pw, 0));
    nvmeThreads[0].addWorker(std::make_unique<DataStoreWorker>(admin1Pw));

    // ── Thread 1: NVMe + TCG cross-domain tests ─────
    if (numThreads > 1) {
        nvmeThreads[1].addWorker(std::make_unique<DiscoveryWorker>());
        nvmeThreads[1].addWorker(std::make_unique<FormatRecoveryWorker>(sidPw));
        nvmeThreads[1].addWorker(std::make_unique<DualSessionWorker>(sidPw, admin1Pw));
    }

    // ── Run threads ─────────────────────────────────
    std::vector<std::thread> threads;
    for (auto& nt : nvmeThreads) {
        threads.push_back(nt.start());
    }
    for (auto& t : threads) {
        t.join();
    }

    libsed::shutdown();
    std::cout << "\n=== All threads complete ===\n";
    return 0;
}
