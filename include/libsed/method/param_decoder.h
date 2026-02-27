#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../codec/token.h"
#include "../codec/token_stream.h"
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

namespace libsed {

/// Helper for decoding method-specific responses
class ParamDecoder {
public:
    ParamDecoder() = default;

    // ── StartSession response ────────────────────────

    struct SessionParams {
        uint32_t tperSessionNumber = 0;
        uint32_t hostSessionNumber = 0;
        Bytes    spChallenge;
        uint32_t tperTransTimeout = 0;
        uint32_t tperInitialTimeout = 0;
    };

    /// Decode SyncSession response parameters
    static Result decodeSyncSession(TokenStream& stream, SessionParams& out);

    // ── Properties response ──────────────────────────

    struct TPerProperties {
        uint32_t maxMethods = 1;
        uint32_t maxSubPackets = 1;
        uint32_t maxPackets = 1;
        uint32_t maxComPacketSize = 1024;
        uint32_t maxResponseComPacketSize = 1024;
        uint32_t maxPacketSize = 1004;
        uint32_t maxIndTokenSize = 968;
        uint32_t maxAggTokenSize = 968;
        uint32_t continuedTokens = 0;
        uint32_t sequenceNumbers = 0;
        uint32_t ackNak = 0;
        uint32_t async = 0;
    };

    static Result decodeProperties(TokenStream& stream, TPerProperties& out);

    // ── Get response ─────────────────────────────────

    /// Decode a Get response into a column-value map
    using ColumnValues = std::unordered_map<uint32_t, Token>;
    static Result decodeGetResponse(TokenStream& stream, ColumnValues& out);

    // ── Locking range info ───────────────────────────

    static Result decodeLockingRange(const ColumnValues& values, LockingRangeInfo& out);

    // ── Generic named-value helpers ──────────────────

    static std::optional<uint64_t> extractUint(const ColumnValues& values, uint32_t col);
    static std::optional<bool>     extractBool(const ColumnValues& values, uint32_t col);
    static std::optional<Bytes>    extractBytes(const ColumnValues& values, uint32_t col);
    static std::optional<std::string> extractString(const ColumnValues& values, uint32_t col);
};

} // namespace libsed
