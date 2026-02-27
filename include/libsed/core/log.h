#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <cstdio>
#include <cstdarg>

namespace libsed {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    None  = 5,
};

/// Global log callback type
using LogCallback = std::function<void(LogLevel level, const char* file, int line, const std::string& msg)>;

class Logger {
public:
    static Logger& instance() {
        static Logger log;
        return log;
    }

    void setLevel(LogLevel level) { level_ = level; }
    LogLevel level() const { return level_; }

    void setCallback(LogCallback cb) { callback_ = std::move(cb); }

    void log(LogLevel level, const char* file, int line, const char* fmt, ...) {
        if (level < level_) return;

        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if (callback_) {
            callback_(level, file, line, std::string(buf));
        } else {
            const char* levelStr = "???";
            switch (level) {
                case LogLevel::Trace: levelStr = "TRC"; break;
                case LogLevel::Debug: levelStr = "DBG"; break;
                case LogLevel::Info:  levelStr = "INF"; break;
                case LogLevel::Warn:  levelStr = "WRN"; break;
                case LogLevel::Error: levelStr = "ERR"; break;
                default: break;
            }
            fprintf(stderr, "[%s] %s:%d: %s\n", levelStr, file, line, buf);
        }
    }

private:
    Logger() = default;
    LogLevel level_ = LogLevel::Info;
    LogCallback callback_;
};

// ── C++17 compliant variadic log macros ─────────────────────
//
// The standard C++17 way to handle zero variadic args:
// Use a template inline function that forwards to printf-style log.
// The macro captures __FILE__ and __LINE__, then delegates.

namespace detail {

/// Log with format args
template <typename... Args>
inline void logFwd(LogLevel level, const char* file, int line,
                   const char* fmt, Args&&... args) {
    Logger::instance().log(level, file, line, fmt, std::forward<Args>(args)...);
}

/// Log with no format args (just a plain string)
inline void logFwd(LogLevel level, const char* file, int line,
                   const char* msg) {
    Logger::instance().log(level, file, line, "%s", msg);
}

} // namespace detail

#define LIBSED_LOG(level, ...)  ::libsed::detail::logFwd(level, __FILE__, __LINE__, __VA_ARGS__)

#define LIBSED_TRACE(...) LIBSED_LOG(::libsed::LogLevel::Trace, __VA_ARGS__)
#define LIBSED_DEBUG(...) LIBSED_LOG(::libsed::LogLevel::Debug, __VA_ARGS__)
#define LIBSED_INFO(...)  LIBSED_LOG(::libsed::LogLevel::Info,  __VA_ARGS__)
#define LIBSED_WARN(...)  LIBSED_LOG(::libsed::LogLevel::Warn,  __VA_ARGS__)
#define LIBSED_ERROR(...) LIBSED_LOG(::libsed::LogLevel::Error, __VA_ARGS__)

} // namespace libsed
