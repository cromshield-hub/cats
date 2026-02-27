/// @file eval_multi_session.cpp
/// @brief Multi-session and multi-thread TCG SED evaluation example.
///
/// Demonstrates:
///   1. Multiple concurrent TCG sessions (Admin SP + Locking SP)
///   2. Multi-threaded evaluation with thread-safe session management
///   3. NVMe commands interleaved with TCG operations (via DI)
///   4. Parallel range configuration across threads
///   5. Stress testing with concurrent lock/unlock
///   6. Session pool pattern for eval frameworks

#include <libsed/eval/eval_api.h>
#include <libsed/transport/nvme_transport.h>
#include <libsed/transport/transport_factory.h>
#include <libsed/security/hash_password.h>
#include <libsed/method/method_uids.h>
#include <libsed/sed_library.h>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <chrono>
#include <cassert>

using namespace libsed;
using namespace libsed::eval;
using Clock = std::chrono::high_resolution_clock;

static std::mutex g_printMutex;

#define TLOG(tid, ...) do { \
    std::lock_guard<std::mutex> lk(g_printMutex); \
    printf("[T%02d] ", tid); \
    printf(__VA_ARGS__); \
    printf("\n"); \
} while(0)

// ════════════════════════════════════════════════════════
//  1. Dual Session: AdminSP + LockingSP simultaneously
// ════════════════════════════════════════════════════════

static void demo_dualSession(EvalApi& api,
                              std::shared_ptr<ITransport> transport,
                              uint16_t comId,
                              const std::string& sidPw,
                              const std::string& admin1Pw) {
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║  1. Dual Session: AdminSP + LockingSP     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    Bytes sidCred = HashPassword::passwordToBytes(sidPw);
    Bytes admin1Cred = HashPassword::passwordToBytes(admin1Pw);

    // Session A: AdminSP as SID
    Session sessionA(transport, comId);
    StartSessionResult ssrA;
    auto rA = api.startSessionWithAuth(sessionA, uid::SP_ADMIN, true,
                                        uid::AUTH_SID, sidCred, ssrA);
    std::cout << "  SessionA (AdminSP/SID): "
              << (rA.ok() ? "OK" : "FAIL")
              << " HSN=" << ssrA.hostSessionNumber
              << " TSN=" << ssrA.tperSessionNumber << "\n";

    // Session B: LockingSP as Admin1
    Session sessionB(transport, comId);
    StartSessionResult ssrB;
    auto rB = api.startSessionWithAuth(sessionB, uid::SP_LOCKING, true,
                                        uid::AUTH_ADMIN1, admin1Cred, ssrB);
    std::cout << "  SessionB (LockingSP/Admin1): "
              << (rB.ok() ? "OK" : "FAIL")
              << " HSN=" << ssrB.hostSessionNumber
              << " TSN=" << ssrB.tperSessionNumber << "\n";

    if (rA.ok() && rB.ok()) {
        // Interleaved operations on both sessions
        RawResult rawA, rawB;

        // Read SP lifecycle from AdminSP
        uint8_t lifecycle = 0;
        api.getSpLifecycle(sessionA, uid::SP_LOCKING, lifecycle, rawA);
        std::cout << "  [A] Locking SP lifecycle = " << (int)lifecycle << "\n";

        // Read locking info from LockingSP
        LockingInfo li;
        api.getLockingInfo(sessionB, 0, li, rawB);
        std::cout << "  [B] GlobalRange: start=" << li.rangeStart
                  << " RLE=" << li.readLockEnabled << "\n";

        // Both sessions active simultaneously — different SPs
        auto infoA = EvalApi::getSessionInfo(sessionA);
        auto infoB = EvalApi::getSessionInfo(sessionB);
        std::cout << "  Both active: A=" << infoA.active << " B=" << infoB.active << "\n";
    }

    if (rA.ok()) api.closeSession(sessionA);
    if (rB.ok()) api.closeSession(sessionB);
}

// ════════════════════════════════════════════════════════
//  2. Multi-threaded: Parallel range query
// ════════════════════════════════════════════════════════

static void demo_parallelRangeQuery(EvalApi& api,
                                     std::shared_ptr<ITransport> transport,
                                     uint16_t comId,
                                     const std::string& admin1Pw,
                                     uint32_t numRanges) {
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║  2. Multi-Thread: Parallel Range Query    ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    Bytes cred = HashPassword::passwordToBytes(admin1Pw);
    std::vector<LockingInfo> results(numRanges);
    std::atomic<uint32_t> successCount{0};
    std::atomic<uint32_t> failCount{0};

    auto start = Clock::now();

    // Each thread opens its own session and reads one range
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < numRanges; i++) {
        threads.emplace_back([&, i]() {
            EvalApi threadApi;
            Session session(transport, comId);
            StartSessionResult ssr;

            auto r = threadApi.startSessionWithAuth(session, uid::SP_LOCKING, false,
                                                     uid::AUTH_ADMIN1, cred, ssr);
            if (r.failed()) {
                TLOG(i, "Session open FAIL: %s", r.message().c_str());
                failCount++;
                return;
            }

            RawResult raw;
            r = threadApi.getLockingInfo(session, i, results[i], raw);
            if (r.ok()) {
                TLOG(i, "Range %u: start=%lu len=%lu RLE=%d",
                     i, results[i].rangeStart, results[i].rangeLength,
                     results[i].readLockEnabled);
                successCount++;
            } else {
                TLOG(i, "Range %u: FAIL %s", i, r.message().c_str());
                failCount++;
            }

            threadApi.closeSession(session);
        });
    }

    for (auto& t : threads) t.join();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - start).count();

    std::cout << "\n  Results: " << successCount << " success, "
              << failCount << " fail, " << elapsed << "ms\n";
}

// ════════════════════════════════════════════════════════
//  3. Stress test: Concurrent lock/unlock cycles
// ════════════════════════════════════════════════════════

static void demo_lockUnlockStress(EvalApi& api,
                                   std::shared_ptr<ITransport> transport,
                                   uint16_t comId,
                                   const std::string& userPw,
                                   uint32_t numThreads,
                                   uint32_t cyclesPerThread) {
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║  3. Stress: Concurrent Lock/Unlock        ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    Bytes cred = HashPassword::passwordToBytes(userPw);
    std::atomic<uint32_t> lockOk{0}, lockFail{0};
    std::atomic<uint32_t> unlockOk{0}, unlockFail{0};

    auto start = Clock::now();

    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < numThreads; t++) {
        threads.emplace_back([&, t]() {
            EvalApi threadApi;

            for (uint32_t c = 0; c < cyclesPerThread; c++) {
                Session session(transport, comId);
                StartSessionResult ssr;
                auto r = threadApi.startSessionWithAuth(session, uid::SP_LOCKING, true,
                                                         uid::AUTH_USER1, cred, ssr);
                if (r.failed()) {
                    TLOG(t, "Cycle %u: session fail", c);
                    lockFail++;
                    continue;
                }

                RawResult raw;

                // Lock
                r = threadApi.setRangeLock(session, 0, true, true, raw);
                if (r.ok()) lockOk++; else lockFail++;

                // Unlock
                r = threadApi.setRangeLock(session, 0, false, false, raw);
                if (r.ok()) unlockOk++; else unlockFail++;

                threadApi.closeSession(session);
            }
            TLOG(t, "Done %u cycles", cyclesPerThread);
        });
    }

    for (auto& t : threads) t.join();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - start).count();

    std::cout << "\n  Lock:   " << lockOk << " ok / " << lockFail << " fail\n";
    std::cout << "  Unlock: " << unlockOk << " ok / " << unlockFail << " fail\n";
    std::cout << "  Total:  " << (lockOk + unlockOk) << " ops in " << elapsed << "ms\n";
    if (elapsed > 0)
        std::cout << "  Rate:   " << ((lockOk + unlockOk) * 1000 / elapsed) << " ops/sec\n";
}

// ════════════════════════════════════════════════════════
//  4. NVMe + TCG interleaved operations
// ════════════════════════════════════════════════════════

static void demo_nvmeInterleaved(EvalApi& api,
                                  std::shared_ptr<ITransport> transport,
                                  uint16_t comId,
                                  const std::string& admin1Pw) {
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║  4. NVMe + TCG Interleaved Operations     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    // Check if NVMe device is available via DI
    INvmeDevice* nvme = EvalApi::getNvmeDevice(transport);
    if (!nvme) {
        std::cout << "  No INvmeDevice available (not DI mode).\n";
        std::cout << "  To use: construct NvmeTransport(shared_ptr<INvmeDevice>)\n";

        // Still demonstrate the API pattern
        std::cout << "\n  Pattern for NVMe + TCG:\n";
        std::cout << "    auto nvme = make_shared<YourLibNvme>(\"/dev/nvme0\");\n";
        std::cout << "    auto tr = make_shared<NvmeTransport>(nvme);\n";
        std::cout << "    // TCG:\n";
        std::cout << "    api.discovery0(tr, info);\n";
        std::cout << "    // NVMe:\n";
        std::cout << "    EvalApi::nvmeIdentify(tr, 1, 0, data);\n";
        return;
    }

    Bytes cred = HashPassword::passwordToBytes(admin1Pw);

    // Step 1: NVMe Identify Controller
    std::cout << "\n  [NVMe] Identify Controller\n";
    Bytes identData;
    auto r = EvalApi::nvmeIdentify(transport, 1, 0, identData);
    if (r.ok() && identData.size() >= 4096) {
        // Parse model name (bytes 24..63)
        std::string model(identData.begin() + 24, identData.begin() + 64);
        std::cout << "    Model: " << model << "\n";
    }

    // Step 2: TCG Discovery
    std::cout << "  [TCG] Discovery\n";
    DiscoveryInfo info;
    api.discovery0(transport, info);

    // Step 3: NVMe Get Log Page (SMART)
    std::cout << "  [NVMe] SMART Log\n";
    Bytes smartData;
    r = EvalApi::nvmeGetLogPage(transport, 0x02, 0xFFFFFFFF, smartData, 512);
    if (r.ok() && smartData.size() >= 2) {
        uint8_t critWarn = smartData[0];
        std::cout << "    Critical Warning: 0x" << std::hex << (int)critWarn << std::dec << "\n";
    }

    // Step 4: TCG Session — read locking state
    std::cout << "  [TCG] Open session, read locking info\n";
    Session session(transport, comId);
    StartSessionResult ssr;
    r = api.startSessionWithAuth(session, uid::SP_LOCKING, false,
                                  uid::AUTH_ADMIN1, cred, ssr);
    if (r.ok()) {
        LockingInfo li;
        RawResult raw;
        api.getLockingInfo(session, 0, li, raw);
        std::cout << "    GlobalRange locked=" << li.readLocked << "\n";
        api.closeSession(session);
    }

    // Step 5: NVMe Get Feature (Power Management)
    std::cout << "  [NVMe] Get Feature (Power Management)\n";
    uint32_t cdw0 = 0;
    Bytes featData;
    r = EvalApi::nvmeGetFeature(transport, 0x02, 0, cdw0, featData);
    if (r.ok()) {
        std::cout << "    Power State: " << (cdw0 & 0x1F) << "\n";
    }
}

// ════════════════════════════════════════════════════════
//  5. Session Pool pattern
// ════════════════════════════════════════════════════════

class SessionPool {
public:
    SessionPool(std::shared_ptr<ITransport> transport, uint16_t comId,
                uint64_t spUid, uint64_t authUid, const Bytes& credential,
                uint32_t poolSize)
        : transport_(transport), comId_(comId), spUid_(spUid),
          authUid_(authUid), credential_(credential) {
        for (uint32_t i = 0; i < poolSize; i++) {
            auto session = std::make_unique<Session>(transport, comId);
            StartSessionResult ssr;
            auto r = api_.startSessionWithAuth(*session, spUid, true,
                                                authUid, credential, ssr);
            if (r.ok()) {
                std::lock_guard<std::mutex> lock(mutex_);
                pool_.push_back(std::move(session));
            }
        }
        std::cout << "  Pool created: " << pool_.size() << " sessions\n";
    }

    ~SessionPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& s : pool_) {
            api_.closeSession(*s);
        }
    }

    /// Borrow a session (blocks if none available)
    std::unique_ptr<Session> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (pool_.empty()) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            lock.lock();
        }
        auto session = std::move(pool_.back());
        pool_.pop_back();
        return session;
    }

    /// Return a session to the pool
    void release(std::unique_ptr<Session> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(session));
    }

    EvalApi& api() { return api_; }

private:
    std::shared_ptr<ITransport> transport_;
    uint16_t comId_;
    uint64_t spUid_;
    uint64_t authUid_;
    Bytes credential_;
    EvalApi api_;
    std::mutex mutex_;
    std::vector<std::unique_ptr<Session>> pool_;
};

static void demo_sessionPool(std::shared_ptr<ITransport> transport,
                              uint16_t comId,
                              const std::string& admin1Pw) {
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║  5. Session Pool: Pre-opened Sessions     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    Bytes cred = HashPassword::passwordToBytes(admin1Pw);

    SessionPool pool(transport, comId,
                     uid::SP_LOCKING, uid::AUTH_ADMIN1, cred,
                     4); // 4 pre-opened sessions

    std::atomic<uint32_t> completed{0};
    auto start = Clock::now();

    // 8 worker threads sharing 4 sessions
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; i++) {
        workers.emplace_back([&, i]() {
            for (int j = 0; j < 5; j++) {
                auto session = pool.acquire();
                RawResult raw;
                LockingInfo li;
                pool.api().getLockingInfo(*session, 0, li, raw);
                TLOG(i, "Job %d: range0.start=%lu", j, li.rangeStart);
                pool.release(std::move(session));
                completed++;
            }
        });
    }

    for (auto& w : workers) w.join();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - start).count();
    std::cout << "\n  Completed " << completed << " jobs in " << elapsed << "ms\n";
}

// ════════════════════════════════════════════════════════
//  6. Concurrent TCG + NVMe on separate threads
// ════════════════════════════════════════════════════════

static void demo_concurrentTcgNvme(EvalApi& api,
                                    std::shared_ptr<ITransport> transport,
                                    uint16_t comId,
                                    const std::string& admin1Pw) {
    std::cout << "\n╔══════════════════════════════════════════╗\n";
    std::cout << "║  6. Concurrent TCG + NVMe Threads         ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    Bytes cred = HashPassword::passwordToBytes(admin1Pw);
    std::atomic<bool> running{true};

    // Thread A: TCG operations
    std::thread tcgThread([&]() {
        EvalApi threadApi;
        int count = 0;
        while (running && count < 10) {
            Session session(transport, comId);
            StartSessionResult ssr;
            auto r = threadApi.startSessionWithAuth(session, uid::SP_LOCKING, false,
                                                     uid::AUTH_ADMIN1, cred, ssr);
            if (r.ok()) {
                LockingInfo li;
                RawResult raw;
                threadApi.getLockingInfo(session, 0, li, raw);
                TLOG(0, "[TCG] iter=%d locked=%d", count, li.readLocked);
                threadApi.closeSession(session);
            }
            count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TLOG(0, "[TCG] Done (%d iterations)", count);
    });

    // Thread B: NVMe operations (if DI available)
    std::thread nvmeThread([&]() {
        INvmeDevice* nvme = EvalApi::getNvmeDevice(transport);
        int count = 0;
        while (running && count < 10) {
            if (nvme) {
                Bytes smartData;
                nvme->getLogPage(0x02, 0xFFFFFFFF, smartData, 512);
                TLOG(1, "[NVMe] iter=%d SMART=%zuB", count, smartData.size());
            } else {
                // Fallback: use transport-level raw recv
                Bytes rawData;
                transport->ifRecv(0x01, 0x0001, rawData, 512);
                TLOG(1, "[NVMe fallback] iter=%d disc=%zuB", count, rawData.size());
            }
            count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        TLOG(1, "[NVMe] Done (%d iterations)", count);
    });

    tcgThread.join();
    running = false;
    nvmeThread.join();
}

// ════════════════════════════════════════════════════════
//  Main
// ════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <device> <sid_password> <admin1_password>"
                  << " [user1_password] [num_ranges] [stress_threads]\n\n";
        std::cerr << "Examples:\n";
        std::cerr << "  " << argv[0] << " /dev/nvme0 sid123 admin123\n";
        std::cerr << "  " << argv[0] << " /dev/nvme0 sid123 admin123 user123 4 8\n";
        return 1;
    }

    std::string device   = argv[1];
    std::string sidPw    = argv[2];
    std::string admin1Pw = argv[3];
    std::string userPw   = (argc > 4) ? argv[4] : admin1Pw;
    uint32_t numRanges   = (argc > 5) ? std::stoul(argv[5]) : 4;
    uint32_t stressT     = (argc > 6) ? std::stoul(argv[6]) : 4;

    libsed::initialize();

    auto transport = TransportFactory::createNvme(device);
    if (!transport || !transport->isOpen()) {
        std::cerr << "Cannot open " << device << "\n";
        return 1;
    }

    EvalApi api;

    // Get ComID
    TcgOption opt;
    api.getTcgOption(transport, opt);
    uint16_t comId = opt.baseComId;
    if (comId == 0) {
        std::cerr << "No valid ComID\n";
        return 1;
    }

    // Exchange properties
    PropertiesResult props;
    api.exchangeProperties(transport, comId, props);

    std::cout << "Device: " << device << "\n";
    std::cout << "ComID:  0x" << std::hex << comId << std::dec << "\n";
    std::cout << "TPer MaxComPacket: " << props.tperMaxComPacketSize << "\n\n";

    // Run demos
    demo_dualSession(api, transport, comId, sidPw, admin1Pw);
    demo_parallelRangeQuery(api, transport, comId, admin1Pw, numRanges);
    demo_lockUnlockStress(api, transport, comId, userPw, stressT, 10);
    demo_nvmeInterleaved(api, transport, comId, admin1Pw);
    demo_sessionPool(transport, comId, admin1Pw);
    demo_concurrentTcgNvme(api, transport, comId, admin1Pw);

    libsed::shutdown();
    std::cout << "\n=== All multi-session demos complete ===\n";
    return 0;
}
