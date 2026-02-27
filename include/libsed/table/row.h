#pragma once

#include "../core/types.h"
#include "../codec/token.h"
#include <unordered_map>
#include <optional>
#include <string>

namespace libsed {

/// Represents a row in a TCG SED table with column-value pairs
class Row {
public:
    Row() = default;
    explicit Row(const Uid& uid) : uid_(uid) {}

    /// Row UID
    const Uid& uid() const { return uid_; }
    void setUid(const Uid& uid) { uid_ = uid; }

    /// Set/get values by column number
    void setUint(uint32_t col, uint64_t val);
    void setBool(uint32_t col, bool val);
    void setBytes(uint32_t col, const Bytes& val);
    void setString(uint32_t col, const std::string& val);

    std::optional<uint64_t>    getUint(uint32_t col) const;
    std::optional<bool>        getBool(uint32_t col) const;
    std::optional<Bytes>       getBytes(uint32_t col) const;
    std::optional<std::string> getString(uint32_t col) const;

    /// Check if a column exists
    bool hasColumn(uint32_t col) const;

    /// Get all columns
    const std::unordered_map<uint32_t, Token>& columns() const { return columns_; }

    /// Load from ParamDecoder column values
    void loadFromColumnValues(const std::unordered_map<uint32_t, Token>& values);

    /// Clear all values
    void clear() { columns_.clear(); }

private:
    Uid uid_;
    std::unordered_map<uint32_t, Token> columns_;
};

} // namespace libsed
