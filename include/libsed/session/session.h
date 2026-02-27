#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../transport/i_transport.h"
#include "../packet/packet_builder.h"
#include "../method/method_result.h"
#include <memory>

namespace libsed {

/// Represents a single TCG SED session with a Security Provider
class Session {
public:
    enum class State {
        Idle,       // Not started
        Starting,   // StartSession sent, waiting for SyncSession
        Active,     // Session active, can send methods
        Closing,    // CloseSession sent
        Closed,     // Session ended
    };

    Session(std::shared_ptr<ITransport> transport, uint16_t comId);
    ~Session();

    // Prevent copy
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Allow move
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;

    // ── Session lifecycle ────────────────────────────

    /// Start a session with the given SP
    Result startSession(const Uid& spUid, bool write,
                        const Uid& hostAuthority = Uid(),
                        const Bytes& hostChallenge = {});

    /// Close the session
    Result closeSession();

    /// Send a method call and receive the result
    Result sendMethod(const Bytes& methodTokens, MethodResult& result);

    // ── Session state ────────────────────────────────

    State state() const { return state_; }
    bool isActive() const { return state_ == State::Active; }
    uint32_t tperSessionNumber() const { return tsn_; }
    uint32_t hostSessionNumber() const { return hsn_; }

    // ── Low-level send/recv ──────────────────────────

    Result sendRaw(const Bytes& comPacketData);
    Result recvRaw(Bytes& comPacketData, uint32_t timeoutMs = 5000);

    // ── Configuration ────────────────────────────────

    void setMaxComPacketSize(uint32_t size) { maxComPacketSize_ = size; }
    uint32_t maxComPacketSize() const { return maxComPacketSize_; }

    void setTimeout(uint32_t ms) { timeoutMs_ = ms; }

private:
    /// Send ComPacket and receive response, handle retries
    Result sendRecv(const Bytes& sendData, Bytes& recvTokens);

    /// Generate next host session number
    static uint32_t nextHostSessionNumber();

    std::shared_ptr<ITransport> transport_;
    PacketBuilder packetBuilder_;
    State state_ = State::Idle;
    uint16_t comId_ = 0;
    uint32_t tsn_ = 0;  // TPer session number
    uint32_t hsn_ = 0;  // Host session number
    uint32_t seqNumber_ = 0;
    uint32_t maxComPacketSize_ = 2048;
    uint32_t timeoutMs_ = 30000;
    static inline uint32_t sessionCounter_ = 1;
};

} // namespace libsed
