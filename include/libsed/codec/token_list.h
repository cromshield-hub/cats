#pragma once

#include "../core/types.h"
#include "token.h"
#include "token_encoder.h"
#include <vector>
#include <variant>
#include <string>

namespace libsed {

/// Convenience builder for constructing named-value token lists
/// Used for method parameters, Get/Set column selections, etc.
class TokenList {
public:
    TokenList() = default;

    /// Add a named unsigned integer
    TokenList& addUint(uint32_t name, uint64_t val) {
        entries_.push_back({name, val, EntryType::Uint});
        return *this;
    }

    /// Add a named boolean
    TokenList& addBool(uint32_t name, bool val) {
        entries_.push_back({name, val ? 1ULL : 0ULL, EntryType::Uint});
        return *this;
    }

    /// Add a named byte array
    TokenList& addBytes(uint32_t name, const Bytes& val) {
        entries_.push_back({name, 0, EntryType::ByteData});
        entries_.back().byteData = val;
        return *this;
    }

    /// Add a named string
    TokenList& addString(uint32_t name, const std::string& val) {
        Bytes data(val.begin(), val.end());
        entries_.push_back({name, 0, EntryType::ByteData});
        entries_.back().byteData = data;
        return *this;
    }

    /// Add a named UID
    TokenList& addUid(uint32_t name, const Uid& val) {
        Bytes data(val.bytes.begin(), val.bytes.end());
        entries_.push_back({name, 0, EntryType::ByteData});
        entries_.back().byteData = data;
        return *this;
    }

    /// Add a named Token (generic)
    TokenList& add(uint32_t name, const Token& tok) {
        if (tok.isByteSequence) {
            return addBytes(name, tok.byteData);
        } else {
            return addUint(name, tok.uintVal);
        }
    }

    /// Encode all entries into an encoder
    void encode(TokenEncoder& encoder) const {
        for (const auto& entry : entries_) {
            encoder.startName();
            encoder.encodeUint(entry.name);
            switch (entry.type) {
                case EntryType::Uint:
                    encoder.encodeUint(entry.uintVal);
                    break;
                case EntryType::ByteData:
                    encoder.encodeBytes(entry.byteData);
                    break;
            }
            encoder.endName();
        }
    }

    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }
    void clear() { entries_.clear(); }

private:
    enum class EntryType { Uint, ByteData };

    struct Entry {
        uint32_t name;
        uint64_t uintVal;
        EntryType type;
        Bytes byteData;
    };

    std::vector<Entry> entries_;
};

} // namespace libsed
