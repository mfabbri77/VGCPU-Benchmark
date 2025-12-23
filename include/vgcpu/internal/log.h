// include/vgcpu/internal/log.h
// Blueprint Reference: [TASK-05.03], [REQ-52..55]
//
// Structured logging interface for VGCPU.
// Supports console and JSONL output formats.
// CRITICAL: No logging inside measured loops per [REQ-54].

#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace vgcpu {
namespace log {

/// Log levels per [REQ-53]
enum class Level {
    kDebug = 0,  ///< Debug information (development only)
    kInfo = 1,   ///< Informational messages
    kWarn = 2,   ///< Warnings (non-critical issues)
    kError = 3,  ///< Errors (recoverable failures)
    kFatal = 4,  ///< Fatal errors (unrecoverable)
    kOff = 5     ///< Logging disabled
};

/// Log output format
enum class Format {
    kConsole,  ///< Human-readable console output
    kJsonl     ///< JSON Lines format for structured logging
};

/// Convert level to string
inline const char* LevelToString(Level level) {
    switch (level) {
        case Level::kDebug:
            return "DEBUG";
        case Level::kInfo:
            return "INFO";
        case Level::kWarn:
            return "WARN";
        case Level::kError:
            return "ERROR";
        case Level::kFatal:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

/// Logger configuration
struct LogConfig {
    Level min_level = Level::kInfo;
    Format format = Format::kConsole;
    bool include_timestamp = true;
    bool include_source = false;
    std::string jsonl_path;  ///< If set, write JSONL to this file
};

/// Global logger singleton
class Logger {
   public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Configure(const LogConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;

        if (!config.jsonl_path.empty()) {
            jsonl_file_.open(config.jsonl_path, std::ios::app);
        }
    }

    void Log(Level level, const char* file, int line, const std::string& message) {
        if (level < config_.min_level) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (config_.format == Format::kJsonl) {
            LogJsonl(level, file, line, message);
        } else {
            LogConsole(level, file, line, message);
        }
    }

    Level GetMinLevel() const { return config_.min_level; }

   private:
    Logger() = default;

    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%H:%M:%S") << '.' << std::setfill('0')
            << std::setw(3) << ms.count();
        return oss.str();
    }

    void LogConsole(Level level, const char* file, int line, const std::string& message) {
        auto& out = (level >= Level::kError) ? std::cerr : std::cout;

        if (config_.include_timestamp) {
            out << "[" << GetTimestamp() << "] ";
        }

        out << "[" << LevelToString(level) << "] ";

        if (config_.include_source && file) {
            out << "[" << file << ":" << line << "] ";
        }

        out << message << "\n";
    }

    void LogJsonl(Level level, const char* file, int line, const std::string& message) {
        std::ostringstream oss;
        oss << R"({"level":")" << LevelToString(level) << R"(")";
        oss << R"(,"msg":")" << EscapeJson(message) << R"(")";

        if (config_.include_timestamp) {
            oss << R"(,"ts":")" << GetTimestamp() << R"(")";
        }

        if (config_.include_source && file) {
            oss << R"(,"file":")" << file << R"(","line":)" << line;
        }

        oss << "}\n";

        std::string output = oss.str();
        std::cout << output;

        if (jsonl_file_.is_open()) {
            jsonl_file_ << output;
            jsonl_file_.flush();
        }
    }

    std::string EscapeJson(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += c;
            }
        }
        return result;
    }

    LogConfig config_;
    std::mutex mutex_;
    std::ofstream jsonl_file_;
};

}  // namespace log
}  // namespace vgcpu

// Logging macros
#define VGCPU_LOG(level, message) \
    vgcpu::log::Logger::Instance().Log(level, __FILE__, __LINE__, message)

#define VGCPU_LOG_DEBUG(msg) VGCPU_LOG(vgcpu::log::Level::kDebug, msg)
#define VGCPU_LOG_INFO(msg) VGCPU_LOG(vgcpu::log::Level::kInfo, msg)
#define VGCPU_LOG_WARN(msg) VGCPU_LOG(vgcpu::log::Level::kWarn, msg)
#define VGCPU_LOG_ERROR(msg) VGCPU_LOG(vgcpu::log::Level::kError, msg)
#define VGCPU_LOG_FATAL(msg) VGCPU_LOG(vgcpu::log::Level::kFatal, msg)

// Conditional logging (avoids evaluation if level is disabled)
#define VGCPU_LOG_IF(level, condition, msg) \
    do {                                    \
        if (condition)                      \
            VGCPU_LOG(level, msg);          \
    } while (0)
