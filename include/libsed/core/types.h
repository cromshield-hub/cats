#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <array>
#include <memory>
#include <functional>
#include <unordered_map>

namespace libsed {

/// Raw byte buffer
using Bytes = std::vector<uint8_t>;

/// Lightweight non-owning view over contiguous bytes (C++17 replacement for std::span)
class ByteSpan {
public:
    constexpr ByteSpan() noexcept : data_(nullptr), size_(0) {}
    constexpr ByteSpan(const uint8_t* data, size_t size) noexcept : data_(data), size_(size) {}
    ByteSpan(const Bytes& v) noexcept : data_(v.data()), size_(v.size()) {}

    constexpr const uint8_t* data() const noexcept { return data_; }
    constexpr size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr const uint8_t* begin() const noexcept { return data_; }
    constexpr const uint8_t* end()   const noexcept { return data_ + size_; }

    constexpr const uint8_t& operator[](size_t i) const { return data_[i]; }

private:
    const uint8_t* data_;
    size_t size_;
};

/// Mutable non-owning view over contiguous bytes
class MutableByteSpan {
public:
    constexpr MutableByteSpan() noexcept : data_(nullptr), size_(0) {}
    constexpr MutableByteSpan(uint8_t* data, size_t size) noexcept : data_(data), size_(size) {}
    MutableByteSpan(Bytes& v) noexcept : data_(v.data()), size_(v.size()) {}

    constexpr uint8_t* data() const noexcept { return data_; }
    constexpr size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr uint8_t* begin() const noexcept { return data_; }
    constexpr uint8_t* end()   const noexcept { return data_ + size_; }

    constexpr uint8_t& operator[](size_t i) const { return data_[i]; }

private:
    uint8_t* data_;
    size_t size_;
};

/// 8-byte UID used throughout TCG SED
struct Uid {
    std::array<uint8_t, 8> bytes{};

    Uid() = default;

    explicit Uid(uint64_t val) {
        for (int i = 7; i >= 0; --i) {
            bytes[i] = static_cast<uint8_t>(val & 0xFF);
            val >>= 8;
        }
    }

    Uid(std::initializer_list<uint8_t> init) {
        size_t i = 0;
        for (auto b : init) {
            if (i < 8) bytes[i++] = b;
        }
    }

    explicit Uid(const Bytes& data) {
        size_t len = std::min(data.size(), size_t(8));
        std::copy(data.begin(), data.begin() + len, bytes.begin());
    }

    uint64_t toUint64() const {
        uint64_t val = 0;
        for (int i = 0; i < 8; ++i) {
            val = (val << 8) | bytes[i];
        }
        return val;
    }

    bool operator==(const Uid& other) const { return bytes == other.bytes; }
    bool operator!=(const Uid& other) const { return bytes != other.bytes; }
    bool operator<(const Uid& other) const { return bytes < other.bytes; }

    bool isNull() const {
        for (auto b : bytes) if (b != 0) return false;
        return true;
    }
};

/// Hash for Uid to use in unordered_map
struct UidHash {
    size_t operator()(const Uid& uid) const {
        return std::hash<uint64_t>{}(uid.toUint64());
    }
};

/// Half-UID (4 bytes) used in some contexts
struct HalfUid {
    std::array<uint8_t, 4> bytes{};

    explicit HalfUid(uint32_t val) {
        for (int i = 3; i >= 0; --i) {
            bytes[i] = static_cast<uint8_t>(val & 0xFF);
            val >>= 8;
        }
    }
};

/// Supported SSC types
enum class SscType : uint8_t {
    Unknown     = 0,
    Enterprise  = 1,
    Opal10      = 2,
    Opal20      = 3,
    Pyrite10    = 4,
    Pyrite20    = 5,
};

/// Transport interface types
enum class TransportType : uint8_t {
    Unknown = 0,
    ATA     = 1,
    NVMe    = 2,
    SCSI    = 3,
};

/// Method status codes (TCG Core Spec)
enum class MethodStatus : uint8_t {
    Success             = 0x00,
    NotAuthorized       = 0x01,
    Obsolete            = 0x02,
    SpBusy              = 0x03,
    SpFailed            = 0x04,
    SpDisabled          = 0x05,
    SpFrozen            = 0x06,
    NoSessionsAvailable = 0x07,
    UniquenessConflict  = 0x08,
    InsufficientSpace   = 0x09,
    InsufficientRows    = 0x0A,
    InvalidParameter    = 0x0C,
    Obsolete2           = 0x0D,
    Obsolete3           = 0x0E,
    TPerMalfunction     = 0x0F,
    TransactionFailure  = 0x10,
    ResponseOverflow    = 0x11,
    AuthorityLockedOut  = 0x12,
    Fail                = 0x3F,
};

/// Cell block for table read/write
struct CellBlock {
    std::optional<uint32_t> startColumn;
    std::optional<uint32_t> endColumn;
    std::optional<uint32_t> startRow;
    std::optional<uint32_t> endRow;
};

/// Locking range information
struct LockingRangeInfo {
    uint32_t rangeId = 0;
    uint64_t rangeStart = 0;
    uint64_t rangeLength = 0;
    bool readLockEnabled = false;
    bool writeLockEnabled = false;
    bool readLocked = false;
    bool writeLocked = false;
};

/// Discovery information summary
struct DiscoveryInfo {
    uint32_t majorVersion = 0;
    uint32_t minorVersion = 0;
    SscType  primarySsc = SscType::Unknown;
    bool     tperPresent = false;
    bool     lockingPresent = false;
    bool     lockingEnabled = false;
    bool     locked = false;
    bool     mbrEnabled = false;
    bool     mbrDone = false;
    uint16_t baseComId = 0;
    uint16_t numComIds = 0;
    uint32_t maxResponseSize = 0;
    uint32_t maxPacketSize = 0;
};

} // namespace libsed
