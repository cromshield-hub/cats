#pragma once

#include "../core/types.h"
#include "../core/error.h"
#include "token.h"
#include <vector>
#include <optional>

namespace libsed {

/// Cursor-based sequential reader for decoded Token sequences
class TokenStream {
public:
    TokenStream() = default;
    explicit TokenStream(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    /// Reset cursor to beginning
    void reset() { pos_ = 0; }

    /// Check if more tokens are available
    bool hasMore() const { return pos_ < tokens_.size(); }

    /// Peek at current token without advancing
    const Token* peek() const {
        if (pos_ >= tokens_.size()) return nullptr;
        return &tokens_[pos_];
    }

    /// Get current token and advance cursor
    const Token* next() {
        if (pos_ >= tokens_.size()) return nullptr;
        return &tokens_[pos_++];
    }

    /// Skip current token
    bool skip() {
        if (pos_ >= tokens_.size()) return false;
        ++pos_;
        return true;
    }

    /// Current position
    size_t position() const { return pos_; }
    void setPosition(size_t pos) { pos_ = pos; }

    /// Total count
    size_t count() const { return tokens_.size(); }

    // ── Typed readers ────────────────────────────────

    /// Read an unsigned integer atom
    std::optional<uint64_t> readUint();

    /// Read a signed integer atom
    std::optional<int64_t> readInt();

    /// Read a byte-sequence atom
    std::optional<Bytes> readBytes();

    /// Read a string (byte sequence interpreted as UTF-8)
    std::optional<std::string> readString();

    /// Read a UID (8-byte atom)
    std::optional<Uid> readUid();

    /// Read a boolean (uint 0 or 1)
    std::optional<bool> readBool();

    // ── Control token matching ───────────────────────

    /// Expect and consume a specific control token
    bool expectStartList();
    bool expectEndList();
    bool expectStartName();
    bool expectEndName();
    bool expectCall();
    bool expectEndOfData();
    bool expectEndOfSession();

    /// Check if current token is a specific control type (without consuming)
    bool isStartList() const;
    bool isEndList() const;
    bool isStartName() const;
    bool isEndName() const;
    bool isCall() const;
    bool isEndOfData() const;
    bool isEndOfSession() const;

    /// Skip an entire list (including nested lists)
    bool skipList();

    /// Skip a named pair { name value }
    bool skipNamedValue();

    // ── Access to all tokens ─────────────────────────
    const std::vector<Token>& tokens() const { return tokens_; }

private:
    bool expectControl(TokenType type);
    bool isControl(TokenType type) const;

    std::vector<Token> tokens_;
    size_t pos_ = 0;
};

} // namespace libsed
