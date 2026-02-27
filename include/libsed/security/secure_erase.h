#pragma once
#include "../core/types.h"
#include <cstring>
#include <string>

namespace libsed {

class SecureErase {
public:
    static void zero(void* ptr, size_t len);
    static void zero(Bytes& data);
    static void zero(std::string& str);
};

class SecureBuffer {
public:
    SecureBuffer() = default;
    explicit SecureBuffer(size_t size);
    explicit SecureBuffer(const Bytes& data) : data_(data) {}
    ~SecureBuffer();
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;
    SecureBuffer(SecureBuffer&& other) noexcept;
    SecureBuffer& operator=(SecureBuffer&& other) noexcept;
    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    Bytes& bytes() { return data_; }
    const Bytes& bytes() const { return data_; }
    void resize(size_t newSize);
    void clear();
private:
    Bytes data_;
};

} // namespace libsed
