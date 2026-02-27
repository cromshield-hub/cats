/// @file eval_step_by_step.cpp
/// @brief Example: Step-by-step TCG SED evaluation using EvalApi.
///
/// Shows how each protocol step can be executed independently
/// with fault injection between steps, raw payload inspection, etc.

#include <libsed/eval/eval_api.h>
#include <libsed/debug/debug.h>
#include <libsed/transport/transport_factory.h>
#include <libsed/sed_library.h>
#include <iostream>
#include <iomanip>

using namespace libsed;
using namespace libsed::eval;
using namespace libsed::debug;

void dumpHex(const Bytes& data, size_t maxBytes = 64) {
    for (size_t i = 0; i < data.size() && i < maxBytes; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << "\n    ";
    }
    if (data.size() > maxBytes) std::cout << "... (" << std::dec << data.size() << " bytes total)";
    std::cout << std::dec << "\n";
}

/// Manual step-by-step evaluation
void manualStepByStep(const std::string& device) {
    std::cout << "\n=== Manual Step-by-Step Eval ===\n";

    auto transport = TransportFactory::createNvme(device);
    if (!transport || !transport->isOpen()) {
        std::cerr << "Cannot open " << device << "\n";
        return;
    }

    EvalApi api;
    uint16_t comId = 0;

    // ── Step 1: Raw Discovery ──
    std::cout << "\n[Step 1] Level 0 Discovery (raw)\n";
    Bytes rawDiscovery;
    auto r = api.discovery0Raw(transport, rawDiscovery);
    std::cout << "  Result: " << r.message() << "\n";
    std::cout << "  Raw response (" << rawDiscovery.size() << " bytes):\n    ";
    dumpHex(rawDiscovery);

    // ── Step 1b: Parsed Discovery ──
    std::cout << "\n[Step 1b] Level 0 Discovery (parsed)\n";
    DiscoveryInfo info;
    r = api.discovery0(transport, info);
    std::cout << "  SSC type: " << static_cast<int>(info.primarySsc) << "\n";
    std::cout << "  Base ComID: 0x" << std::hex << info.baseComId << std::dec << "\n";
    std::cout << "  Locking enabled: " << info.lockingEnabled << "\n";
    std::cout << "  Locked: " << info.locked << "\n";
    comId = info.baseComId;

    // ── Step 2: Properties ──
    std::cout << "\n[Step 2] Properties Exchange\n";
    PropertiesResult props;
    r = api.exchangeProperties(transport, comId, props);
    std::cout << "  Result: " << r.message() << "\n";
    std::cout << "  TPer MaxComPacket: " << props.tperMaxComPacketSize << "\n";
    std::cout << "  TPer MaxPacket: " << props.tperMaxPacketSize << "\n";
    std::cout << "  Send payload (" << props.raw.rawSendPayload.size() << " bytes):\n    ";
    dumpHex(props.raw.rawSendPayload);

    // ── Step 3: StartSession (Admin SP, read-only, no auth) ──
    std::cout << "\n[Step 3] StartSession (Admin SP, read, Anybody)\n";
    Session session(transport, comId);
    session.setMaxComPacketSize(props.tperMaxComPacketSize);
    StartSessionResult ssr;
    r = api.startSession(session, uid::SP_ADMIN, false, ssr);
    std::cout << "  Result: " << r.message() << "\n";
    std::cout << "  HSN: " << ssr.hostSessionNumber << "\n";
    std::cout << "  TSN: " << ssr.tperSessionNumber << "\n";

    if (r.failed()) return;

    // ── Step 4: Read MSID (C_PIN table, no auth needed) ──
    std::cout << "\n[Step 4] Get C_PIN(MSID)\n";
    Bytes msidPin;
    RawResult raw;
    r = api.getCPin(session, uid::CPIN_MSID, msidPin, raw);
    std::cout << "  Result: " << r.message() << "\n";
    if (r.ok()) {
        std::cout << "  MSID PIN (" << msidPin.size() << " bytes):\n    ";
        dumpHex(msidPin);
    }

    // ── Step 5: Close session ──
    std::cout << "\n[Step 5] CloseSession\n";
    r = api.closeSession(session);
    std::cout << "  Result: " << r.message() << "\n";

    // ── Step 6: Custom negative test — Discovery with wrong protocol ID ──
    std::cout << "\n[Step 6] Discovery with wrong Protocol ID (negative test)\n";
    Bytes badDiscovery;
    r = api.discovery0Custom(transport, 0x05, 0x0001, badDiscovery);
    std::cout << "  Result: " << r.message() << "\n";
    std::cout << "  Response (" << badDiscovery.size() << " bytes):\n    ";
    dumpHex(badDiscovery);
}

/// Step-by-step with fault injection between steps
void faultInjectedEval(const std::string& device) {
    std::cout << "\n=== Fault-Injected Step-by-Step Eval ===\n";

    auto& tc = TestContext::instance();
    tc.enable();

    TestSession ts("fault_eval");

    // Arm: corrupt the 3rd IF-RECV (which should be the SyncSession response)
    ts.fault(
        FaultBuilder("corrupt_sync_session")
            .at(FaultPoint::AfterIfRecv)
            .corrupt(12, 0xFF)  // corrupt byte 12 of response
            .once()
    );

    auto transport = TransportFactory::createNvme(device);
    if (!transport || !transport->isOpen()) return;

    EvalApi api;

    // Discovery works fine (fault not yet triggered if hitCountdown allows)
    DiscoveryInfo info;
    api.discovery0(transport, info);

    // StartSession — the SyncSession response will be corrupted
    Session session(transport, info.baseComId);
    StartSessionResult ssr;
    auto r = api.startSession(session, uid::SP_ADMIN, false, ssr);
    std::cout << "  StartSession with corrupted SyncSession: " << r.message() << "\n";

    // Print trace
    for (auto& ev : ts.trace()) {
        std::cout << "  [trace] " << ev.tag << ": " << ev.detail << "\n";
    }

    tc.disable();
}

/// Use the step observer to log each step of ownership
void observedOwnership(const std::string& device) {
    std::cout << "\n=== Observed Ownership Sequence ===\n";

    auto transport = TransportFactory::createNvme(device);
    if (!transport || !transport->isOpen()) return;

    DiscoveryInfo info;
    EvalApi api;
    api.discovery0(transport, info);

    sequence::takeOwnershipStepByStep(
        transport, info.baseComId, "new_password_123",
        [](const std::string& step, const RawResult& raw) -> bool {
            std::cout << "  [" << step << "] "
                      << "transport=" << static_cast<int>(raw.transportError)
                      << " protocol=" << static_cast<int>(raw.protocolError)
                      << "\n";
            return true; // continue to next step
        }
    );
}

int main(int argc, char* argv[]) {
    std::string device = (argc > 1) ? argv[1] : "/dev/nvme0";

    libsed::initialize();

    manualStepByStep(device);

    if (argc > 1) {
        faultInjectedEval(device);
        observedOwnership(device);
    }

    libsed::shutdown();
    return 0;
}
