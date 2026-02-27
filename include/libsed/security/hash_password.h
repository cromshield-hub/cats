#pragma once

#include "../core/types.h"
#include <string>

namespace libsed {

/// Password hashing utilities for TCG SED
class HashPassword {
public:
    /// Hash a password using PBKDF2-HMAC-SHA256
    /// @param password  User password
    /// @param salt      Salt bytes (typically serial number + user name)
    /// @param iterations  PBKDF2 iterations (recommend >= 75000)
    /// @param keyLen    Output key length (typically 32)
    /// @return derived key bytes
    static Bytes pbkdf2Sha256(const std::string& password,
                               const Bytes& salt,
                               uint32_t iterations = 75000,
                               uint32_t keyLen = 32);

    /// Hash password with drive serial as salt (convenience)
    static Bytes hashForDrive(const std::string& password,
                               const std::string& serialNumber,
                               uint32_t iterations = 75000);

    /// Simple: convert string password to bytes (no hashing, for testing)
    static Bytes passwordToBytes(const std::string& password);

    /// SHA-256 hash
    static Bytes sha256(const uint8_t* data, size_t len);
    static Bytes sha256(const Bytes& data) { return sha256(data.data(), data.size()); }

    /// HMAC-SHA-256
    static Bytes hmacSha256(const Bytes& key, const Bytes& data);
};

} // namespace libsed
