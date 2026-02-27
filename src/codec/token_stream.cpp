#include "libsed/codec/token_stream.h"

namespace libsed {

std::optional<uint64_t> TokenStream::readUint() {
    const Token* t = next();
    if (!t || !t->isAtom() || t->isByteSequence || t->isSigned) return std::nullopt;
    return t->getUint();
}

std::optional<int64_t> TokenStream::readInt() {
    const Token* t = next();
    if (!t || !t->isAtom() || t->isByteSequence) return std::nullopt;
    if (t->isSigned) return t->getInt();
    return static_cast<int64_t>(t->getUint());
}

std::optional<Bytes> TokenStream::readBytes() {
    const Token* t = next();
    if (!t || !t->isAtom() || !t->isByteSequence) return std::nullopt;
    return t->getBytes();
}

std::optional<std::string> TokenStream::readString() {
    auto bytes = readBytes();
    if (!bytes) return std::nullopt;
    return std::string(bytes->begin(), bytes->end());
}

std::optional<Uid> TokenStream::readUid() {
    auto bytes = readBytes();
    if (!bytes || bytes->size() != 8) return std::nullopt;
    Uid uid;
    std::copy(bytes->begin(), bytes->end(), uid.bytes.begin());
    return uid;
}

std::optional<bool> TokenStream::readBool() {
    auto val = readUint();
    if (!val) return std::nullopt;
    return *val != 0;
}

bool TokenStream::expectControl(TokenType type) {
    const Token* t = next();
    return t && t->type == type;
}

bool TokenStream::isControl(TokenType type) const {
    const Token* t = peek();
    return t && t->type == type;
}

bool TokenStream::expectStartList()   { return expectControl(TokenType::StartList); }
bool TokenStream::expectEndList()     { return expectControl(TokenType::EndList); }
bool TokenStream::expectStartName()   { return expectControl(TokenType::StartName); }
bool TokenStream::expectEndName()     { return expectControl(TokenType::EndName); }
bool TokenStream::expectCall()        { return expectControl(TokenType::Call); }
bool TokenStream::expectEndOfData()   { return expectControl(TokenType::EndOfData); }
bool TokenStream::expectEndOfSession(){ return expectControl(TokenType::EndOfSession); }

bool TokenStream::isStartList() const   { return isControl(TokenType::StartList); }
bool TokenStream::isEndList() const     { return isControl(TokenType::EndList); }
bool TokenStream::isStartName() const   { return isControl(TokenType::StartName); }
bool TokenStream::isEndName() const     { return isControl(TokenType::EndName); }
bool TokenStream::isCall() const        { return isControl(TokenType::Call); }
bool TokenStream::isEndOfData() const   { return isControl(TokenType::EndOfData); }
bool TokenStream::isEndOfSession() const{ return isControl(TokenType::EndOfSession); }

bool TokenStream::skipList() {
    if (!expectStartList()) return false;
    int depth = 1;
    while (depth > 0 && hasMore()) {
        const Token* t = next();
        if (!t) return false;
        if (t->type == TokenType::StartList) ++depth;
        else if (t->type == TokenType::EndList) --depth;
    }
    return depth == 0;
}

bool TokenStream::skipNamedValue() {
    if (!expectStartName()) return false;
    // Skip name
    if (!skip()) return false;
    // Skip value (could be a list)
    const Token* t = peek();
    if (!t) return false;
    if (t->type == TokenType::StartList) {
        if (!skipList()) return false;
    } else {
        if (!skip()) return false;
    }
    return expectEndName();
}

} // namespace libsed
