#pragma once

#include <cstdint>
#include <string>
#include <system_error>
#include <stdexcept>

namespace libsed {

/// Error codes for the TCG SED library
enum class ErrorCode : int {
    Success = 0,

    // Transport errors (100-199)
    TransportNotAvailable = 100,
    TransportOpenFailed   = 101,
    TransportSendFailed   = 102,
    TransportRecvFailed   = 103,
    TransportTimeout      = 104,
    TransportInvalidDevice = 105,

    // Protocol errors (200-299)
    InvalidToken          = 200,
    InvalidPacket         = 201,
    InvalidSubPacket      = 202,
    InvalidComPacket      = 203,
    BufferTooSmall        = 204,
    BufferOverflow        = 205,
    UnexpectedToken       = 206,
    MalformedResponse     = 207,
    ProtocolError         = 208,

    // Session errors (300-399)
    SessionNotStarted     = 300,
    SessionAlreadyActive  = 301,
    SessionClosed         = 302,
    SessionSyncFailed     = 303,
    NoSessionAvailable    = 304,

    // Method errors (400-499)
    MethodNotAuthorized   = 401,
    MethodSpBusy          = 403,
    MethodSpFailed        = 404,
    MethodSpDisabled      = 405,
    MethodSpFrozen        = 406,
    MethodInvalidParam    = 412,
    MethodTPerMalfunction = 415,
    MethodFailed          = 463,

    // Discovery errors (500-599)
    DiscoveryFailed       = 500,
    DiscoveryInvalidData  = 501,
    UnsupportedSsc        = 502,
    FeatureNotFound       = 503,

    // Auth errors (600-699)
    AuthFailed            = 600,
    AuthLockedOut         = 601,
    InvalidCredential     = 602,

    // General errors (900-999)
    NotImplemented        = 900,
    InvalidArgument       = 901,
    InternalError         = 999,
};

/// Error category for TCG SED
class SedErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "libsed"; }

    std::string message(int ev) const override {
        switch (static_cast<ErrorCode>(ev)) {
            case ErrorCode::Success:               return "Success";
            case ErrorCode::TransportNotAvailable:  return "Transport not available";
            case ErrorCode::TransportOpenFailed:    return "Failed to open transport";
            case ErrorCode::TransportSendFailed:    return "IF-SEND failed";
            case ErrorCode::TransportRecvFailed:    return "IF-RECV failed";
            case ErrorCode::TransportTimeout:       return "Transport timeout";
            case ErrorCode::InvalidToken:           return "Invalid token";
            case ErrorCode::InvalidPacket:          return "Invalid packet";
            case ErrorCode::BufferTooSmall:         return "Buffer too small";
            case ErrorCode::SessionNotStarted:      return "Session not started";
            case ErrorCode::SessionAlreadyActive:   return "Session already active";
            case ErrorCode::MethodNotAuthorized:    return "Method not authorized";
            case ErrorCode::MethodFailed:           return "Method failed";
            case ErrorCode::DiscoveryFailed:        return "Discovery failed";
            case ErrorCode::UnsupportedSsc:         return "Unsupported SSC";
            case ErrorCode::AuthFailed:             return "Authentication failed";
            case ErrorCode::AuthLockedOut:          return "Authority locked out";
            case ErrorCode::NotImplemented:         return "Not implemented";
            case ErrorCode::InvalidArgument:        return "Invalid argument";
            case ErrorCode::InternalError:          return "Internal error";
            default:                                return "Unknown error";
        }
    }

    static const SedErrorCategory& instance() {
        static SedErrorCategory cat;
        return cat;
    }
};

inline std::error_code make_error_code(ErrorCode e) {
    return {static_cast<int>(e), SedErrorCategory::instance()};
}

/// Result type wrapping an error code
class Result {
public:
    Result() : code_(ErrorCode::Success) {}
    Result(ErrorCode code) : code_(code) {}

    bool ok() const { return code_ == ErrorCode::Success; }
    bool failed() const { return code_ != ErrorCode::Success; }
    explicit operator bool() const { return ok(); }

    ErrorCode code() const { return code_; }
    std::string message() const { return SedErrorCategory::instance().message(static_cast<int>(code_)); }

    static Result success() { return Result(ErrorCode::Success); }

private:
    ErrorCode code_;
};

/// Exception type for critical failures
class SedException : public std::runtime_error {
public:
    explicit SedException(ErrorCode code)
        : std::runtime_error(SedErrorCategory::instance().message(static_cast<int>(code)))
        , code_(code) {}

    SedException(ErrorCode code, const std::string& detail)
        : std::runtime_error(SedErrorCategory::instance().message(static_cast<int>(code)) + ": " + detail)
        , code_(code) {}

    ErrorCode code() const { return code_; }

private:
    ErrorCode code_;
};

} // namespace libsed

// Register as std::error_code compatible
template<>
struct std::is_error_code_enum<libsed::ErrorCode> : std::true_type {};
