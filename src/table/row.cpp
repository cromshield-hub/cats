#include "libsed/table/row.h"

namespace libsed {

void Row::setUint(uint32_t col, uint64_t val) {
    columns_[col] = Token::makeUint(val);
}

void Row::setBool(uint32_t col, bool val) {
    columns_[col] = Token::makeUint(val ? 1 : 0);
}

void Row::setBytes(uint32_t col, const Bytes& val) {
    columns_[col] = Token::makeBytes(val);
}

void Row::setString(uint32_t col, const std::string& val) {
    Bytes data(val.begin(), val.end());
    columns_[col] = Token::makeBytes(data);
}

std::optional<uint64_t> Row::getUint(uint32_t col) const {
    auto it = columns_.find(col);
    if (it == columns_.end() || it->second.isByteSequence) return std::nullopt;
    return it->second.getUint();
}

std::optional<bool> Row::getBool(uint32_t col) const {
    auto val = getUint(col);
    if (!val) return std::nullopt;
    return *val != 0;
}

std::optional<Bytes> Row::getBytes(uint32_t col) const {
    auto it = columns_.find(col);
    if (it == columns_.end() || !it->second.isByteSequence) return std::nullopt;
    return it->second.getBytes();
}

std::optional<std::string> Row::getString(uint32_t col) const {
    auto bytes = getBytes(col);
    if (!bytes) return std::nullopt;
    return std::string(bytes->begin(), bytes->end());
}

bool Row::hasColumn(uint32_t col) const {
    return columns_.find(col) != columns_.end();
}

void Row::loadFromColumnValues(const std::unordered_map<uint32_t, Token>& values) {
    columns_ = values;
}

} // namespace libsed
