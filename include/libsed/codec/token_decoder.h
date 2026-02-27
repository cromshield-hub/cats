#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "token.h"
#include <vector>

namespace libsed {

/// Decodes TCG SED token byte stream into Token sequence
class TokenDecoder {
public:
    TokenDecoder() = default;

    /// Decode all tokens from a byte buffer
    Result decode(const uint8_t* data, size_t len);
    Result decode(ByteSpan data) { return decode(data.data(), data.size()); }
    Result decode(const Bytes& data) { return decode(data.data(), data.size()); }

    /// Get decoded tokens
    const std::vector<Token>& tokens() const { return tokens_; }
    std::vector<Token>&& releaseTokens() { return std::move(tokens_); }

    /// Number of decoded tokens
    size_t count() const { return tokens_.size(); }

    /// Access a specific token
    const Token& at(size_t index) const { return tokens_.at(index); }
    const Token& operator[](size_t index) const { return tokens_[index]; }

    /// Clear decoded tokens
    void clear() { tokens_.clear(); }

private:
    /// Decode a single token starting at data[offset], returns bytes consumed
    size_t decodeOne(const uint8_t* data, size_t len, size_t offset, Token& out);

    /// Decode a tiny atom (single byte, top bit = 0)
    size_t decodeTinyAtom(const uint8_t* data, size_t offset, Token& out);

    /// Decode a short atom (top 2 bits = 10)
    size_t decodeShortAtom(const uint8_t* data, size_t len, size_t offset, Token& out);

    /// Decode a medium atom (top 3 bits = 110)
    size_t decodeMediumAtom(const uint8_t* data, size_t len, size_t offset, Token& out);

    /// Decode a long atom (top 5 bits = 11100)
    size_t decodeLongAtom(const uint8_t* data, size_t len, size_t offset, Token& out);

    std::vector<Token> tokens_;
};

} // namespace libsed
