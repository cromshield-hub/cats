#include "libsed/codec/token_decoder.h"
#include "libsed/core/endian.h"
#include "libsed/core/log.h"

namespace libsed {

Result TokenDecoder::decode(const uint8_t* data, size_t len) {
    tokens_.clear();
    size_t offset = 0;

    while (offset < len) {
        Token token;
        size_t consumed = decodeOne(data, len, offset, token);
        if (consumed == 0) {
            LIBSED_ERROR("Failed to decode token at offset %zu", offset);
            return ErrorCode::InvalidToken;
        }
        tokens_.push_back(std::move(token));
        offset += consumed;
    }

    return ErrorCode::Success;
}

size_t TokenDecoder::decodeOne(const uint8_t* data, size_t len,
                                 size_t offset, Token& out) {
    if (offset >= len) return 0;

    uint8_t byte = data[offset];

    // Check for control tokens (0xF0-0xFF range)
    if (byte >= 0xF0) {
        out = Token::makeControl(static_cast<TokenType>(byte));
        return 1;
    }

    // Tiny atom: top bit = 0 (0x00-0x7F)
    if ((byte & 0x80) == 0) {
        return decodeTinyAtom(data, offset, out);
    }

    // Short atom: top 2 bits = 10 (0x80-0xBF)
    if ((byte & 0xC0) == 0x80) {
        return decodeShortAtom(data, len, offset, out);
    }

    // Medium atom: top 3 bits = 110 (0xC0-0xDF)
    if ((byte & 0xE0) == 0xC0) {
        return decodeMediumAtom(data, len, offset, out);
    }

    // Long atom: top 5 bits = 11100 (0xE0-0xE3)
    if ((byte & 0xFC) == 0xE0) {
        return decodeLongAtom(data, len, offset, out);
    }

    // Unknown token
    LIBSED_WARN("Unknown token byte 0x%02X at offset %zu", byte, offset);
    out.type = TokenType::Invalid;
    return 1;
}

size_t TokenDecoder::decodeTinyAtom(const uint8_t* data, size_t offset, Token& out) {
    uint8_t byte = data[offset];

    if (byte & 0x40) {
        // Signed tiny atom: 01xxxxxx
        int8_t val = static_cast<int8_t>(byte & 0x3F);
        // Sign-extend from 6 bits
        if (val & 0x20) val |= static_cast<int8_t>(0xC0);
        out = Token::makeInt(val);
        out.type = TokenType::TinyAtom;
    } else {
        // Unsigned tiny atom: 00xxxxxx
        out = Token::makeUint(byte & 0x3F);
        out.type = TokenType::TinyAtom;
    }

    return 1;
}

size_t TokenDecoder::decodeShortAtom(const uint8_t* data, size_t len,
                                       size_t offset, Token& out) {
    uint8_t header = data[offset];
    bool isByte   = (header & 0x20) != 0;
    bool isSigned = (header & 0x10) != 0;
    size_t dataLen = header & 0x0F;

    if (offset + 1 + dataLen > len) {
        LIBSED_ERROR("Short atom truncated at offset %zu", offset);
        return 0;
    }

    const uint8_t* payload = data + offset + 1;

    if (isByte) {
        out = Token::makeBytes(payload, dataLen);
        out.type = TokenType::ShortAtom;
    } else if (isSigned) {
        int64_t val = Endian::decodeSigned(payload, dataLen);
        out = Token::makeInt(val);
        out.type = TokenType::ShortAtom;
    } else {
        uint64_t val = Endian::decodeUnsigned(payload, dataLen);
        out = Token::makeUint(val);
        out.type = TokenType::ShortAtom;
    }

    return 1 + dataLen;
}

size_t TokenDecoder::decodeMediumAtom(const uint8_t* data, size_t len,
                                        size_t offset, Token& out) {
    if (offset + 2 > len) return 0;

    uint8_t header0 = data[offset];
    uint8_t header1 = data[offset + 1];

    bool isByte   = (header0 & 0x10) != 0;
    bool isSigned = (header0 & 0x08) != 0;
    size_t dataLen = (static_cast<size_t>(header0 & 0x07) << 8) | header1;

    if (offset + 2 + dataLen > len) {
        LIBSED_ERROR("Medium atom truncated at offset %zu", offset);
        return 0;
    }

    const uint8_t* payload = data + offset + 2;

    if (isByte) {
        out = Token::makeBytes(payload, dataLen);
        out.type = TokenType::MediumAtom;
    } else if (isSigned) {
        int64_t val = Endian::decodeSigned(payload, dataLen);
        out = Token::makeInt(val);
        out.type = TokenType::MediumAtom;
    } else {
        uint64_t val = Endian::decodeUnsigned(payload, dataLen);
        out = Token::makeUint(val);
        out.type = TokenType::MediumAtom;
    }

    return 2 + dataLen;
}

size_t TokenDecoder::decodeLongAtom(const uint8_t* data, size_t len,
                                      size_t offset, Token& out) {
    if (offset + 4 > len) return 0;

    uint8_t header0 = data[offset];
    bool isByte   = (header0 & 0x02) != 0;
    bool isSigned = (header0 & 0x01) != 0;

    size_t dataLen = (static_cast<size_t>(data[offset + 1]) << 16) |
                     (static_cast<size_t>(data[offset + 2]) << 8) |
                     static_cast<size_t>(data[offset + 3]);

    if (offset + 4 + dataLen > len) {
        LIBSED_ERROR("Long atom truncated at offset %zu", offset);
        return 0;
    }

    const uint8_t* payload = data + offset + 4;

    if (isByte) {
        out = Token::makeBytes(payload, dataLen);
        out.type = TokenType::LongAtom;
    } else if (isSigned) {
        int64_t val = Endian::decodeSigned(payload, dataLen);
        out = Token::makeInt(val);
        out.type = TokenType::LongAtom;
    } else {
        uint64_t val = Endian::decodeUnsigned(payload, dataLen);
        out = Token::makeUint(val);
        out.type = TokenType::LongAtom;
    }

    return 4 + dataLen;
}

} // namespace libsed
