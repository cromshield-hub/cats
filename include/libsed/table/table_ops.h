#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "../session/session.h"
#include "../method/method_result.h"
#include "../method/param_decoder.h"
#include <unordered_map>
#include <optional>

namespace libsed {

/// High-level table operations (Get, Set, Next, etc.)
class TableOps {
public:
    explicit TableOps(Session& session) : session_(session) {}

    /// Get column values from a row
    Result get(const Uid& objectUid, const CellBlock& cellBlock,
               ParamDecoder::ColumnValues& values);

    /// Get all columns from a row
    Result getAll(const Uid& objectUid, ParamDecoder::ColumnValues& values);

    /// Get a single column value
    Result getColumn(const Uid& objectUid, uint32_t column, Token& value);

    /// Get a single uint column
    Result getUint(const Uid& objectUid, uint32_t column, uint64_t& value);

    /// Get a single bytes column
    Result getBytes(const Uid& objectUid, uint32_t column, Bytes& value);

    /// Set column values for a row
    Result set(const Uid& objectUid, const ParamDecoder::ColumnValues& values);

    /// Set a single uint column
    Result setUint(const Uid& objectUid, uint32_t column, uint64_t value);

    /// Set a single bool column
    Result setBool(const Uid& objectUid, uint32_t column, bool value);

    /// Set a single bytes column
    Result setBytes(const Uid& objectUid, uint32_t column, const Bytes& value);

    /// Set C_PIN (password)
    Result setPin(const Uid& cpinUid, const Bytes& pin);
    Result setPin(const Uid& cpinUid, const std::string& pin);

    /// Authenticate to an authority
    Result authenticate(const Uid& authority, const Bytes& credential);
    Result authenticate(const Uid& authority, const std::string& password);

    /// GenKey on a row
    Result genKey(const Uid& objectUid);

    /// Enumerate rows in a table (Next method)
    Result next(const Uid& tableUid, const Uid& startRow,
                std::vector<Uid>& rows, uint32_t count = 0);

    /// Revert SP
    Result revertSP(const Uid& spUid);

    /// Activate SP
    Result activate(const Uid& spUid);

    /// Erase (for Enterprise bands)
    Result erase(const Uid& objectUid);

    /// Get a random number from the TPer
    Result getRandom(Bytes& randomData, uint32_t count = 32);

private:
    /// Internal: send method and parse result
    Result sendAndParse(const Bytes& methodTokens, MethodResult& result);

    Session& session_;
};

} // namespace libsed
