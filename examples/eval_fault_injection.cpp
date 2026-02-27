/// @file eval_fault_injection.cpp
/// @brief Example: Using the Debug/Test Layer for NVMe SED evaluation.
///
/// This example demonstrates how an evaluation platform engineer would
/// use TestContext, FaultBuilder, TestSession, workarounds, and trace
/// to test edge cases on an NVMe TCG SED drive.

#include <libsed/sed_library.h>
#include <libsed/debug/debug.h>
#include <iostream>
#include <cassert>

using namespace libsed;
using namespace libsed::debug;

/// Scenario 1: Verify that the library retries on SP_BUSY responses.
void scenario_sp_busy_retry(const std::string& device) {
    std::cout << "\n=== Scenario: SP_BUSY retry ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    // Create a scoped test session — everything cleans up on return
    TestSession ts("sp_busy_retry");

    // Arm: the first 3 sends will return SP_BUSY error
    ts.fault(
        FaultBuilder("busy_3x")
            .at(FaultPoint::AfterRecvMethod)
            .returnError(ErrorCode::MethodSpBusy)
            .times(3)
    );

    // Activate the retry workaround so the library should retry
    ts.workaround(workaround::kRetryOnSpBusy);

    // Run the actual operation
    auto sed = SedDevice::open(device);
    if (!sed) {
        std::cerr << "  [skip] Cannot open device\n";
        return;
    }

    // The library should internally retry and eventually succeed
    // (after the 3 injected failures are consumed)
    auto r = sed->takeOwnership("test_password");

    // Check counters
    std::cout << "  transport.send = " << ts.counter("transport.send") << "\n";
    std::cout << "  transport.recv = " << ts.counter("transport.recv") << "\n";

    // Print trace
    for (const auto& ev : ts.trace()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            ev.timestamp.time_since_epoch()).count();
        std::cout << "  [" << ms << "ms] " << ev.tag << " : " << ev.detail
                  << " (rc=" << static_cast<int>(ev.result) << ")\n";
    }
}

/// Scenario 2: Corrupt a response and verify error handling.
void scenario_corrupt_response(const std::string& device) {
    std::cout << "\n=== Scenario: Corrupt response ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("corrupt_recv");

    // Corrupt byte 0 of the first received packet — this should cause
    // a ComPacket parse failure somewhere downstream
    ts.fault(
        FaultBuilder("corrupt_hdr")
            .at(FaultPoint::AfterIfRecv)
            .corrupt(0, 0xFF)   // XOR first byte
            .once()
    );

    auto sed = SedDevice::open(device);
    if (!sed) {
        std::cerr << "  [skip] Cannot open device\n";
        return;
    }

    auto r = sed->takeOwnership("test_password");
    std::cout << "  Result: " << r.message()
              << " (expected failure due to corruption)\n";
    assert(r.failed());
}

/// Scenario 3: Replace the Discovery response with a crafted payload.
void scenario_fake_discovery() {
    std::cout << "\n=== Scenario: Fake discovery payload ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("fake_discovery");

    // Build a fake Discovery response that claims Pyrite 2.0
    Bytes fakeDiscovery(256, 0);
    // (In a real eval you'd craft this byte-for-byte per the spec)
    fakeDiscovery[0] = 0x00; fakeDiscovery[1] = 0x00;
    fakeDiscovery[2] = 0x00; fakeDiscovery[3] = 100; // length

    ts.fault(
        FaultBuilder("fake_disc_resp")
            .at(FaultPoint::AfterDiscovery)
            .replaceWith(fakeDiscovery)
            .once()
    );

    std::cout << "  Fault armed: will inject fake Pyrite 2.0 discovery\n";
}

/// Scenario 4: Timeout extension workaround for slow enterprise drives.
void scenario_timeout_workaround() {
    std::cout << "\n=== Scenario: Timeout extension workaround ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("slow_enterprise");

    // Set a longer timeout for this session
    ts.config("timeout_extend_ms", uint64_t{120000}); // 2 minutes
    ts.workaround(workaround::kExtendTimeout);

    std::cout << "  Timeout extended to "
              << tc.configUint("timeout_extend_ms", "slow_enterprise") << " ms\n";
}

/// Scenario 5: Use callback fault for custom protocol-level injection.
void scenario_custom_callback() {
    std::cout << "\n=== Scenario: Custom callback injection ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("custom_inject");

    int callCount = 0;
    ts.fault(
        FaultBuilder("log_every_send")
            .at(FaultPoint::BeforeIfSend)
            .callback([&](Bytes& payload) -> Result {
                callCount++;
                std::cout << "  [callback] Send #" << callCount
                          << ", payload size = " << payload.size() << "\n";
                // Could modify payload here for protocol-level testing
                return ErrorCode::Success; // don't block
            })
            .always()
    );

    std::cout << "  Callback fault armed (will log every IF-SEND)\n";
}

/// Scenario 6: Global config for all sessions.
void scenario_global_config() {
    std::cout << "\n=== Scenario: Global config ===\n";

    auto& tc = TestContext::instance();
    tc.reset();
    tc.enable();

    // Set global test parameters
    tc.setGlobalConfig("max_retries", int64_t{5});
    tc.setGlobalConfig("skip_revert_confirm", true);
    tc.setGlobalConfig("inject_serial", std::string{"FAKE_SN_12345"});

    // Any session can read these
    assert(tc.configInt("max_retries") == 5);
    assert(tc.configBool("skip_revert_confirm") == true);
    assert(tc.configStr("inject_serial") == "FAKE_SN_12345");

    // Per-session override
    tc.setConfig("max_retries", "enterprise_test", ConfigValue(int64_t{10}));
    assert(tc.configInt("max_retries", "enterprise_test") == 10); // overridden
    assert(tc.configInt("max_retries", "opal_test") == 5);         // falls back

    tc.disable();
    std::cout << "  Global and per-session config verified\n";
}

int main(int argc, char* argv[]) {
    std::string device = (argc > 1) ? argv[1] : "/dev/nvme0";

    libsed::initialize();

    scenario_global_config();
    scenario_timeout_workaround();
    scenario_custom_callback();
    scenario_fake_discovery();

    // These need a real device
    if (argc > 1) {
        scenario_sp_busy_retry(device);
        scenario_corrupt_response(device);
    } else {
        std::cout << "\n[Skipping device-dependent scenarios. "
                     "Pass a device path to run them.]\n";
    }

    TestContext::instance().reset();
    libsed::shutdown();

    std::cout << "\n=== All evaluation scenarios complete ===\n";
    return 0;
}
