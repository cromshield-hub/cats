#pragma once

#include "../core/types.h"
#include <variant>
#include <string>
#include <vector>

namespace libsed {

/// TCG SED Token types
enum class TokenType : uint8_t {
    // Atom types
    TinyAtom        = 0x00,   // 0xxxxxxx - unsigned 6-bit or signed 6-bit
    ShortAtom       = 0x01,   // 10xxxxxx - up to 15 bytes
    MediumAtom      = 0x02,   // 110xxxxx - up to 2047 bytes
    LongAtom        = 0x03,   // 11100xxx - up to 16M bytes

    // Control tokens (0xF0-0xFF)
    StartList       = 0xF0,
    EndList         = 0xF1,
    StartName       = 0xF2,
    EndName         = 0xF3,
    Call            = 0xF8,
    EndOfData       = 0xF9,
    EndOfSession    = 0xFA,
    StartTransaction = 0xFB,
    EndTransaction  = 0xFC,
    EmptyAtom       = 0xFF,   // zero-length atom (empty)

    Invalid         = 0xFE,
};

/// Represents a single decoded token
struct Token {
    TokenType type = TokenType::Invalid;
    bool isSigned = false;
    bool isByteSequence = false;

    // Payload: either integer or byte data
    union {
        uint64_t  uintVal;
        int64_t   intVal;
    };
    Bytes byteData;

    Token() : uintVal(0) {}

    // Factory methods
    static Token makeUint(uint64_t val) {
        Token t;
        t.type = TokenType::TinyAtom;
        t.isSigned = false;
        t.isByteSequence = false;
        t.uintVal = val;
        return t;
    }

    static Token makeInt(int64_t val) {
        Token t;
        t.type = TokenType::TinyAtom;
        t.isSigned = true;
        t.isByteSequence = false;
        t.intVal = val;
        return t;
    }

    static Token makeBytes(const Bytes& data) {
        Token t;
        t.type = TokenType::ShortAtom;
        t.isSigned = false;
        t.isByteSequence = true;
        t.byteData = data;
        t.uintVal = 0;
        return t;
    }

    static Token makeBytes(const uint8_t* data, size_t len) {
        Token t;
        t.type = TokenType::ShortAtom;
        t.isSigned = false;
        t.isByteSequence = true;
        t.byteData.assign(data, data + len);
        t.uintVal = 0;
        return t;
    }

    static Token makeControl(TokenType ctrl) {
        Token t;
        t.type = ctrl;
        t.uintVal = 0;
        return t;
    }

    // Accessors
    uint64_t getUint() const { return uintVal; }
    int64_t  getInt() const { return intVal; }
    const Bytes& getBytes() const { return byteData; }

    bool isAtom() const {
        return type == TokenType::TinyAtom ||
               type == TokenType::ShortAtom ||
               type == TokenType::MediumAtom ||
               type == TokenType::LongAtom ||
               type == TokenType::EmptyAtom;
    }

    bool isControl() const { return !isAtom(); }

    std::string toString() const;
};

} // namespace libsed
