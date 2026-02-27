#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../codec/token_encoder.h"
#include "../codec/token_list.h"
#include "method_uids.h"
#include <vector>
#include <optional>

namespace libsed {

/// Builds a TCG SED method call token stream
/// Structure: CALL InvokingID MethodID [ params... ] EndOfData [ status ]
class MethodCall {
public:
    MethodCall() = default;
    MethodCall(const Uid& invokingId, const Uid& methodId)
        : invokingId_(invokingId), methodId_(methodId) {}

    MethodCall(uint64_t invokingId, uint64_t methodId)
        : invokingId_(Uid(invokingId)), methodId_(Uid(methodId)) {}

    /// Set the invoking object UID
    void setInvokingId(const Uid& uid) { invokingId_ = uid; }
    void setInvokingId(uint64_t uid) { invokingId_ = Uid(uid); }

    /// Set the method UID
    void setMethodId(const Uid& uid) { methodId_ = uid; }
    void setMethodId(uint64_t uid) { methodId_ = Uid(uid); }

    /// Access to parameter encoder (add tokens between StartList/EndList)
    TokenEncoder& params() { return paramEncoder_; }
    const TokenEncoder& params() const { return paramEncoder_; }

    /// Set pre-built parameters (replaces current)
    void setParams(const Bytes& paramTokens) { paramEncoder_.clear(); paramEncoder_.appendRaw(paramTokens); }

    /// Build complete method call token stream
    /// Returns: CALL uid uid [ params ] EndOfData [ 0 0 0 ]
    Bytes build() const;

    /// Build method call for session manager (uses SMUID as invoking ID)
    static Bytes buildSmCall(uint64_t smMethodUid, const Bytes& paramTokens);

    // ── Convenience factory methods ──────────────────

    /// Build GET method call with CellBlock
    static Bytes buildGet(const Uid& objectUid, const CellBlock& cellBlock = {});

    /// Build SET method call with values
    static Bytes buildSet(const Uid& objectUid, const TokenList& values);

    /// Build Authenticate method call
    static Bytes buildAuthenticate(const Uid& authorityUid, const Bytes& credential);

    /// Build GenKey method call
    static Bytes buildGenKey(const Uid& objectUid);

    /// Build Revert method call on SP
    static Bytes buildRevertSP(const Uid& spUid);

    /// Build Activate method call
    static Bytes buildActivate(const Uid& spUid);

    /// Build Erase method call
    static Bytes buildErase(const Uid& objectUid);

private:
    Uid invokingId_;
    Uid methodId_;
    TokenEncoder paramEncoder_;
};

} // namespace libsed
