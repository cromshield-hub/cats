#include "libsed/session/session_manager.h"
#include "libsed/method/method_call.h"
#include "libsed/method/method_result.h"
#include "libsed/method/param_encoder.h"
#include "libsed/method/param_decoder.h"
#include "libsed/method/method_uids.h"
#include "libsed/table/table_ops.h"
#include "libsed/core/uid.h"
#include "libsed/core/log.h"
#include "libsed/security/hash_password.h"

namespace libsed {

SessionManager::SessionManager(std::shared_ptr<ITransport> transport, uint16_t comId)
    : transport_(std::move(transport))
    , comId_(comId) {}

Result SessionManager::openSession(const Uid& spUid, bool write,
                                     std::unique_ptr<Session>& session,
                                     const Uid& authority,
                                     const Bytes& credential) {
    session = std::make_unique<Session>(transport_, comId_);
    session->setMaxComPacketSize(tperMaxComPacketSize_);

    Uid hostAuth = authority;
    Bytes hostChallenge = credential;

    auto r = session->startSession(spUid, write, hostAuth, hostChallenge);
    if (r.failed()) {
        session.reset();
        return r;
    }

    return ErrorCode::Success;
}

Result SessionManager::openSessionAndAuthenticate(
    const Uid& spUid,
    const Uid& authority,
    const std::string& password,
    std::unique_ptr<Session>& session) {

    // First open session without auth
    auto r = openSession(spUid, true, session);
    if (r.failed()) return r;

    // Then authenticate within the session
    Bytes credential = HashPassword::passwordToBytes(password);

    TableOps ops(*session);
    r = ops.authenticate(authority, credential);
    if (r.failed()) {
        session->closeSession();
        session.reset();
        return r;
    }

    return ErrorCode::Success;
}

Result SessionManager::closeSession(std::unique_ptr<Session>& session) {
    if (!session) return ErrorCode::SessionNotStarted;
    auto r = session->closeSession();
    session.reset();
    return r;
}

Result SessionManager::withSession(const Uid& spUid, bool write,
                                     const Uid& authority, const Bytes& credential,
                                     const MethodFunc& func) {
    std::unique_ptr<Session> session;
    Result r;

    if (!authority.isNull() && !credential.empty()) {
        // Open with inline auth
        r = openSession(spUid, write, session, authority, credential);
    } else if (!authority.isNull()) {
        // Open without auth, then authenticate separately
        r = openSession(spUid, write, session);
        if (r.ok()) {
            TableOps ops(*session);
            r = ops.authenticate(authority, credential);
        }
    } else {
        r = openSession(spUid, write, session);
    }

    if (r.failed()) return r;

    r = func(*session);

    session->closeSession();
    return r;
}

Result SessionManager::exchangeProperties() {
    LIBSED_DEBUG("Exchanging Properties with TPer");

    Session tempSession(transport_, comId_);

    // Build Properties method call
    ParamEncoder::HostProperties hostProps;
    hostProps.maxComPacketSize = 65536;
    hostProps.maxResponseComPacketSize = 65536;
    hostProps.maxPacketSize = 65516;
    hostProps.maxIndTokenSize = 65480;
    hostProps.maxAggTokenSize = 65480;

    Bytes params = ParamEncoder::encodeProperties(hostProps);
    Bytes methodTokens = MethodCall::buildSmCall(method::SM_PROPERTIES, params);

    // Send via session manager (TSN=0, HSN=0)
    PacketBuilder pb;
    pb.setComId(comId_);
    Bytes sendData = pb.buildSessionManagerPacket(methodTokens);

    auto r = transport_->ifSend(0x01, comId_,
                                 ByteSpan(sendData.data(), sendData.size()));
    if (r.failed()) return r;

    // Receive
    Bytes recvBuffer;
    r = transport_->ifRecv(0x01, comId_, recvBuffer, 65536);
    if (r.failed()) return r;

    // Parse
    PacketBuilder::ParsedResponse parsed;
    r = pb.parseResponse(recvBuffer, parsed);
    if (r.failed()) return r;

    MethodResult result;
    r = result.parse(parsed.tokenPayload);
    if (r.failed()) return r;

    if (!result.isSuccess()) return result.toResult();

    // Decode TPer properties
    ParamDecoder::TPerProperties tperProps;
    auto stream = result.resultStream();

    // Response has two lists: HostProperties echo, TPerProperties
    // Skip host properties echo (first list)
    if (stream.isStartList()) {
        stream.skipList();
    }

    // Parse TPer properties (second list)
    if (stream.isStartList()) {
        stream.expectStartList();
        ParamDecoder::decodeProperties(stream, tperProps);
        stream.expectEndList();
    }

    tperMaxComPacketSize_ = tperProps.maxComPacketSize;
    tperMaxPacketSize_ = tperProps.maxPacketSize;
    tperMaxTokenSize_ = tperProps.maxIndTokenSize;

    LIBSED_INFO("TPer MaxComPacketSize=%u MaxPacketSize=%u",
                 tperMaxComPacketSize_, tperMaxPacketSize_);

    return ErrorCode::Success;
}

} // namespace libsed
