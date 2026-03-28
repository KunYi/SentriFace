#ifndef SENTRIFACE_LOGGING_SYSTEM_LOGGER_HPP_
#define SENTRIFACE_LOGGING_SYSTEM_LOGGER_HPP_

#include <cstdint>
#include <sstream>
#include <string>

namespace sentriface::logging {

enum class LogLevel : std::uint8_t {
  kOff = 0,
  kError = 1,
  kWarn = 2,
  kInfo = 3,
  kDebug = 4,
};

enum class LogBackend : std::uint8_t {
  kDummy = 0,
  kStdout = 1,
  kFile = 2,
  kFileBinary = 3,
  kNetworkRpc = 4,
};

enum class LogCompression : std::uint8_t {
  kNone = 0,
  kGzip = 1,
  kZstd = 2,
};

struct LoggerConfig {
  bool enabled = false;
  LogLevel level = LogLevel::kInfo;
  LogBackend backend = LogBackend::kDummy;
  LogCompression compression = LogCompression::kNone;
  std::string module_filter;
  std::string file_path;
  std::string rpc_endpoint;
};

struct LogMessage {
  LogLevel level = LogLevel::kInfo;
  std::uint32_t timestamp_ms = 0;
  std::string module;
  std::string component;
  std::string message;
};

struct LogResult {
  bool ok = false;
  bool emitted = false;
  std::string detail;
};

class SystemLogger {
 public:
  explicit SystemLogger(const LoggerConfig& config = LoggerConfig {});

  bool IsEnabled(LogLevel level) const;
  bool IsModuleEnabled(const std::string& module) const;
  bool IsReady() const;
  LogResult Log(const LogMessage& message) const;
  LogResult LogNow(LogLevel level,
                   const std::string& module,
                   const std::string& message) const;

  LoggerConfig config() const;

 private:
  LogResult EmitStdout(const LogMessage& message) const;
  LogResult EmitFile(const LogMessage& message) const;
  LogResult EmitBinaryFile(const LogMessage& message) const;
  LogResult EmitNetworkRpc(const LogMessage& message) const;

  LoggerConfig config_;
};

LoggerConfig LoadLoggerConfigFromEnv(
    const LoggerConfig& defaults = LoggerConfig {});
const char* LogLevelName(LogLevel level);
const char* LogBackendName(LogBackend backend);
const char* LogCompressionName(LogCompression compression);
std::uint32_t CurrentTimestampMs();

}  // namespace sentriface::logging

#ifndef SENTRIFACE_LOG_TAG
#define SENTRIFACE_LOG_TAG "SentriFace"
#endif

#define SENTRIFACE_LOG_STREAM_TAG(logger_expr, level_value, tag_value, stream_expr)         \
  do {                                                                                   \
    auto& _sentriface_logger = (logger_expr);                                            \
    const char* _sentriface_tag = (tag_value);                                           \
    if (_sentriface_logger.IsEnabled((level_value)) &&                                   \
        _sentriface_logger.IsModuleEnabled(_sentriface_tag)) {                           \
      std::ostringstream _sentriface_log_oss;                                            \
      _sentriface_log_oss << stream_expr;                                                \
      static_cast<void>(_sentriface_logger.LogNow(                                       \
          (level_value), _sentriface_tag, _sentriface_log_oss.str()));                   \
    }                                                                                    \
  } while (0)

#define SENTRIFACE_LOG_STREAM(logger_expr, level_value, stream_expr)                        \
  SENTRIFACE_LOG_STREAM_TAG((logger_expr), (level_value), SENTRIFACE_LOG_TAG, stream_expr)

#define SENTRIFACE_LOGE(logger_expr, stream_expr)                                           \
  SENTRIFACE_LOG_STREAM((logger_expr), ::sentriface::logging::LogLevel::kError, stream_expr)

#define SENTRIFACE_LOGW(logger_expr, stream_expr)                                           \
  SENTRIFACE_LOG_STREAM((logger_expr), ::sentriface::logging::LogLevel::kWarn, stream_expr)

#define SENTRIFACE_LOGI(logger_expr, stream_expr)                                           \
  SENTRIFACE_LOG_STREAM((logger_expr), ::sentriface::logging::LogLevel::kInfo, stream_expr)

#define SENTRIFACE_LOGD(logger_expr, stream_expr)                                           \
  SENTRIFACE_LOG_STREAM((logger_expr), ::sentriface::logging::LogLevel::kDebug, stream_expr)

#define SENTRIFACE_LOGE_TAG(logger_expr, tag_value, stream_expr)                            \
  SENTRIFACE_LOG_STREAM_TAG(                                                                \
      (logger_expr), ::sentriface::logging::LogLevel::kError, (tag_value), stream_expr)

#define SENTRIFACE_LOGW_TAG(logger_expr, tag_value, stream_expr)                            \
  SENTRIFACE_LOG_STREAM_TAG(                                                                \
      (logger_expr), ::sentriface::logging::LogLevel::kWarn, (tag_value), stream_expr)

#define SENTRIFACE_LOGI_TAG(logger_expr, tag_value, stream_expr)                            \
  SENTRIFACE_LOG_STREAM_TAG(                                                                \
      (logger_expr), ::sentriface::logging::LogLevel::kInfo, (tag_value), stream_expr)

#define SENTRIFACE_LOGD_TAG(logger_expr, tag_value, stream_expr)                            \
  SENTRIFACE_LOG_STREAM_TAG(                                                                \
      (logger_expr), ::sentriface::logging::LogLevel::kDebug, (tag_value), stream_expr)

#endif  // SENTRIFACE_LOGGING_SYSTEM_LOGGER_HPP_
