#include "libsed/security/secure_erase.h"

#include <cstring>

#ifdef _MSC_VER
#include <windows.h>
#endif

namespace libsed {

void SecureErase::zero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;

#if defined(_MSC_VER)
    SecureZeroMemory(ptr, len);
#elif defined(__STDC_LIB_EXT1__)
    memset_s(ptr, len, 0, len);
#else
    // Volatile pointer prevents the compiler from optimizing out the memset
    volatile uint8_t* vp = static_cast<volatile uint8_t*>(ptr);
    for (size_t i = 0; i < len; ++i) {
        vp[i] = 0;
    }
    // Memory barrier to prevent reordering
    __asm__ __volatile__("" ::: "memory");
#endif
}

void SecureErase::zero(Bytes& data) {
    if (!data.empty()) {
        zero(data.data(), data.size());
    }
}

void SecureErase::zero(std::string& str) {
    if (!str.empty()) {
        zero(str.data(), str.size());
    }
}

SecureBuffer::SecureBuffer(size_t size) : data_(size, 0) {}

SecureBuffer::~SecureBuffer() {
    SecureErase::zero(data_);
}

SecureBuffer::SecureBuffer(SecureBuffer&& other) noexcept
    : data_(std::move(other.data_)) {}

SecureBuffer& SecureBuffer::operator=(SecureBuffer&& other) noexcept {
    if (this != &other) {
        SecureErase::zero(data_);
        data_ = std::move(other.data_);
    }
    return *this;
}

void SecureBuffer::resize(size_t newSize) {
    if (newSize < data_.size()) {
        // Zero the bytes being removed
        SecureErase::zero(data_.data() + newSize, data_.size() - newSize);
    }
    data_.resize(newSize);
}

void SecureBuffer::clear() {
    SecureErase::zero(data_);
    data_.clear();
}

} // namespace libsed
