#include "logging/system_logger.hpp"

#include <cstdlib>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace sentriface::logging {

namespace {

constexpr char kBinaryLogMagic[4] = {'F', 'L', 'O', 'G'};
constexpr std::uint16_t kBinaryLogVersion = 1U;

bool ReadEnvBool(const char* name, bool default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::atoi(value) != 0;
}

std::string ReadEnvString(const char* name, const std::string& default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::string(value);
}

std::vector<std::string> SplitCsv(const std::string& text) {
  std::vector<std::string> out;
  std::string current;
  for (const char ch : text) {
    if (ch == ',') {
      if (!current.empty()) {
        out.push_back(current);
        current.clear();
      }
      continue;
    }
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    out.push_back(current);
  }
  return out;
}

LogLevel ParseLevel(const std::string& value, LogLevel fallback) {
  if (value == "off") {
    return LogLevel::kOff;
  }
  if (value == "error") {
    return LogLevel::kError;
  }
  if (value == "warn" || value == "warning") {
    return LogLevel::kWarn;
  }
  if (value == "info") {
    return LogLevel::kInfo;
  }
  if (value == "debug") {
    return LogLevel::kDebug;
  }
  return fallback;
}

LogBackend ParseBackend(const std::string& value, LogBackend fallback) {
  if (value == "dummy") {
    return LogBackend::kDummy;
  }
  if (value == "stdout") {
    return LogBackend::kStdout;
  }
  if (value == "file" || value == "local_file") {
    return LogBackend::kFile;
  }
  if (value == "file_binary" || value == "binary") {
    return LogBackend::kFileBinary;
  }
  if (value == "rpc" || value == "network" || value == "network_rpc") {
    return LogBackend::kNetworkRpc;
  }
  return fallback;
}

LogCompression ParseCompression(const std::string& value,
                                LogCompression fallback) {
  if (value == "none" || value.empty()) {
    return LogCompression::kNone;
  }
  if (value == "gzip" || value == "gz") {
    return LogCompression::kGzip;
  }
  if (value == "zstd" || value == "zst") {
    return LogCompression::kZstd;
  }
  return fallback;
}

std::string FormatWallTimestamp(std::uint32_t wall_sec, std::uint16_t wall_msec) {
  const std::time_t raw_time = static_cast<std::time_t>(wall_sec);
  std::tm local_tm {};
  localtime_r(&raw_time, &local_tm);

  std::ostringstream oss;
  oss << std::put_time(&local_tm, "%m-%d %H:%M:%S")
      << '.'
      << std::setw(3) << std::setfill('0') << wall_msec;
  return oss.str();
}

std::string FormatMessage(const LogMessage& message,
                          std::uint32_t wall_sec,
                          std::uint16_t wall_msec) {
  const std::string& module =
      !message.module.empty() ? message.module : message.component;
  const char* level_name = LogLevelName(message.level);
  const char priority = level_name[0] != '\0'
      ? static_cast<char>(std::toupper(level_name[0]))
      : 'I';
  std::ostringstream oss;
  oss << FormatWallTimestamp(wall_sec, wall_msec)
      << ' '
      << '+'
      << message.timestamp_ms
      << "ms"
      << ' ' << priority
      << '/' << (module.empty() ? "SentriFace" : module)
      << ": " << message.message;
  return oss.str();
}

const char* CompressionCommand(LogCompression compression) {
  switch (compression) {
    case LogCompression::kNone:
      return nullptr;
    case LogCompression::kGzip:
      return "gzip -c";
    case LogCompression::kZstd:
      return "zstd -q -c";
  }
  return nullptr;
}

bool IsFileMissingOrEmpty(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.is_open()) {
    return true;
  }
  return in.tellg() <= 0;
}

bool WriteAllBytes(const std::string& path, const std::vector<char>& data) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return out.good();
}

bool ReadAllBytes(const std::string& path, std::vector<char>& data) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool MakeTempFilePath(char* path_template) {
  const int fd = mkstemp(path_template);
  if (fd < 0) {
    return false;
  }
  close(fd);
  return true;
}

bool CompressChunk(LogCompression compression,
                   const std::vector<char>& input,
                   std::vector<char>& output,
                   std::string& error) {
  if (compression == LogCompression::kNone) {
    output = input;
    return true;
  }

  const char* command = CompressionCommand(compression);
  if (command == nullptr) {
    error = "unsupported_compression";
    return false;
  }

  char input_template[] = "/tmp/sentriface_log_in_XXXXXX";
  char output_template[] = "/tmp/sentriface_log_out_XXXXXX";
  if (!MakeTempFilePath(input_template) || !MakeTempFilePath(output_template)) {
    error = "tempfile_create_failed";
    return false;
  }

  const std::string input_path(input_template);
  const std::string output_path(output_template);
  const auto cleanup = [&]() {
    std::remove(input_path.c_str());
    std::remove(output_path.c_str());
  };

  if (!WriteAllBytes(input_path, input)) {
    error = "tempfile_write_failed";
    cleanup();
    return false;
  }

  std::ostringstream oss;
  oss << command << " < " << input_path << " > " << output_path;
  if (std::system(oss.str().c_str()) != 0) {
    error = "compression_command_failed";
    cleanup();
    return false;
  }

  if (!ReadAllBytes(output_path, output)) {
    error = "compressed_output_read_failed";
    cleanup();
    return false;
  }

  cleanup();
  return true;
}

bool AppendBytes(const std::string& path,
                 const std::vector<char>& data,
                 std::string& error) {
  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out.is_open()) {
    error = "file_backend_open_failed";
    return false;
  }
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!out.good()) {
    error = "file_backend_write_failed";
    return false;
  }
  return true;
}

std::uint32_t CurrentWallSeconds() {
  using Clock = std::chrono::system_clock;
  const auto now = Clock::now();
  const auto seconds =
      std::chrono::time_point_cast<std::chrono::seconds>(now);
  return static_cast<std::uint32_t>(seconds.time_since_epoch().count());
}

std::uint16_t CurrentWallMillisecondsRemainder() {
  using Clock = std::chrono::system_clock;
  const auto now = Clock::now();
  const auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count();
  return static_cast<std::uint16_t>(millis % 1000);
}

}  // namespace

SystemLogger::SystemLogger(const LoggerConfig& config) : config_(config) {}

bool SystemLogger::IsEnabled(LogLevel level) const {
  if (!config_.enabled) {
    return false;
  }
  return static_cast<int>(level) <= static_cast<int>(config_.level) &&
         level != LogLevel::kOff;
}

bool SystemLogger::IsModuleEnabled(const std::string& module) const {
  if (config_.module_filter.empty()) {
    return true;
  }
  const std::vector<std::string> modules = SplitCsv(config_.module_filter);
  if (modules.empty()) {
    return true;
  }
  const std::string module_name = module.empty() ? "SentriFace" : module;
  for (const auto& allowed : modules) {
    if (allowed == "*" || allowed == module_name) {
      return true;
    }
  }
  return false;
}

bool SystemLogger::IsReady() const {
  if (!config_.enabled || config_.level == LogLevel::kOff) {
    return true;
  }

  switch (config_.backend) {
    case LogBackend::kDummy:
    case LogBackend::kStdout:
      return true;
    case LogBackend::kFile:
      return !config_.file_path.empty();
    case LogBackend::kFileBinary:
      return !config_.file_path.empty();
    case LogBackend::kNetworkRpc:
      return !config_.rpc_endpoint.empty();
  }
  return false;
}

LogResult SystemLogger::Log(const LogMessage& message) const {
  LogResult result;
  result.ok = true;
  result.emitted = false;
  result.detail = "filtered";

  const std::string module =
      !message.module.empty() ? message.module : message.component;
  if (!IsEnabled(message.level) || !IsModuleEnabled(module)) {
    return result;
  }

  switch (config_.backend) {
    case LogBackend::kDummy:
      result.detail = "dummy";
      return result;
    case LogBackend::kStdout:
      return EmitStdout(message);
    case LogBackend::kFile:
      return EmitFile(message);
    case LogBackend::kFileBinary:
      return EmitBinaryFile(message);
    case LogBackend::kNetworkRpc:
      return EmitNetworkRpc(message);
  }
  result.ok = false;
  result.detail = "unknown_backend";
  return result;
}

LogResult SystemLogger::LogNow(LogLevel level,
                               const std::string& module,
                               const std::string& message) const {
  LogMessage log_message;
  log_message.level = level;
  log_message.timestamp_ms = CurrentTimestampMs();
  log_message.module = module;
  log_message.message = message;
  return Log(log_message);
}

LoggerConfig SystemLogger::config() const { return config_; }

LogResult SystemLogger::EmitStdout(const LogMessage& message) const {
  const std::uint32_t wall_sec = CurrentWallSeconds();
  const std::uint16_t wall_msec = CurrentWallMillisecondsRemainder();
  const std::string formatted = FormatMessage(message, wall_sec, wall_msec);
  std::cout << formatted << '\n';
  std::cout.flush();

  LogResult result;
  result.ok = true;
  result.emitted = true;
  result.detail = formatted;
  return result;
}

LogResult SystemLogger::EmitFile(const LogMessage& message) const {
  LogResult result;
  if (config_.file_path.empty()) {
    result.ok = false;
    result.emitted = false;
    result.detail = "file_backend_not_configured";
    return result;
  }

  const std::uint32_t wall_sec = CurrentWallSeconds();
  const std::uint16_t wall_msec = CurrentWallMillisecondsRemainder();
  const std::string formatted = FormatMessage(message, wall_sec, wall_msec);
  std::vector<char> chunk(formatted.begin(), formatted.end());
  chunk.push_back('\n');

  std::vector<char> compressed;
  if (!CompressChunk(config_.compression, chunk, compressed, result.detail)) {
    result.ok = false;
    result.emitted = false;
    return result;
  }
  if (!AppendBytes(config_.file_path, compressed, result.detail)) {
    result.ok = false;
    result.emitted = false;
    return result;
  }
  result.ok = true;
  result.emitted = true;
  result.detail = formatted;
  return result;
}

LogResult SystemLogger::EmitBinaryFile(const LogMessage& message) const {
  LogResult result;
  if (config_.file_path.empty()) {
    result.ok = false;
    result.emitted = false;
    result.detail = "binary_file_backend_not_configured";
    return result;
  }

  const std::string module =
      !message.module.empty() ? message.module : message.component;
  if (module.size() > 0xFFFFU || message.message.size() > 0xFFFFU) {
    result.ok = false;
    result.emitted = false;
    result.detail = "binary_record_too_large";
    return result;
  }

  std::vector<char> raw_chunk;
  if (config_.compression == LogCompression::kNone &&
      IsFileMissingOrEmpty(config_.file_path)) {
    raw_chunk.insert(raw_chunk.end(), kBinaryLogMagic, kBinaryLogMagic + 4);
    raw_chunk.push_back(static_cast<char>(kBinaryLogVersion & 0xFF));
    raw_chunk.push_back(static_cast<char>((kBinaryLogVersion >> 8) & 0xFF));
    raw_chunk.push_back(0);
    raw_chunk.push_back(0);
  } else if (config_.compression != LogCompression::kNone &&
             IsFileMissingOrEmpty(config_.file_path)) {
    raw_chunk.insert(raw_chunk.end(), kBinaryLogMagic, kBinaryLogMagic + 4);
    raw_chunk.push_back(static_cast<char>(kBinaryLogVersion & 0xFF));
    raw_chunk.push_back(static_cast<char>((kBinaryLogVersion >> 8) & 0xFF));
    raw_chunk.push_back(0);
    raw_chunk.push_back(0);
  }

  const auto append_u16 = [&](std::uint16_t value) {
    raw_chunk.push_back(static_cast<char>(value & 0xFF));
    raw_chunk.push_back(static_cast<char>((value >> 8) & 0xFF));
  };
  const auto append_u32 = [&](std::uint32_t value) {
    raw_chunk.push_back(static_cast<char>(value & 0xFF));
    raw_chunk.push_back(static_cast<char>((value >> 8) & 0xFF));
    raw_chunk.push_back(static_cast<char>((value >> 16) & 0xFF));
    raw_chunk.push_back(static_cast<char>((value >> 24) & 0xFF));
  };

  append_u32(message.timestamp_ms);
  append_u32(CurrentWallSeconds());
  append_u16(CurrentWallMillisecondsRemainder());
  raw_chunk.push_back(static_cast<char>(std::toupper(LogLevelName(message.level)[0])));
  raw_chunk.push_back(0);
  append_u16(static_cast<std::uint16_t>(module.size()));
  append_u16(static_cast<std::uint16_t>(message.message.size()));
  append_u16(0U);
  raw_chunk.insert(raw_chunk.end(), module.begin(), module.end());
  raw_chunk.insert(raw_chunk.end(), message.message.begin(), message.message.end());

  std::vector<char> compressed;
  if (!CompressChunk(config_.compression, raw_chunk, compressed, result.detail)) {
    result.ok = false;
    result.emitted = false;
    return result;
  }
  if (!AppendBytes(config_.file_path, compressed, result.detail)) {
    result.ok = false;
    result.emitted = false;
    return result;
  }

  result.ok = true;
  result.emitted = true;
  result.detail = "binary_record_written";
  return result;
}

LogResult SystemLogger::EmitNetworkRpc(const LogMessage& message) const {
  LogResult result;
  if (config_.rpc_endpoint.empty()) {
    result.ok = false;
    result.emitted = false;
    result.detail = "network_rpc_backend_not_configured";
    return result;
  }

  const std::uint32_t wall_sec = CurrentWallSeconds();
  const std::uint16_t wall_msec = CurrentWallMillisecondsRemainder();
  result.ok = true;
  result.emitted = true;
  result.detail = "network_rpc_stub:" + FormatMessage(message, wall_sec, wall_msec);
  return result;
}

LoggerConfig LoadLoggerConfigFromEnv(const LoggerConfig& defaults) {
  LoggerConfig config = defaults;
  config.enabled = ReadEnvBool("SENTRIFACE_LOG_ENABLE", defaults.enabled);
  config.level = ParseLevel(
      ReadEnvString("SENTRIFACE_LOG_LEVEL", LogLevelName(defaults.level)),
      defaults.level);
  config.backend = ParseBackend(
      ReadEnvString("SENTRIFACE_LOG_BACKEND", LogBackendName(defaults.backend)),
      defaults.backend);
  config.compression = ParseCompression(
      ReadEnvString("SENTRIFACE_LOG_COMPRESSION",
                    LogCompressionName(defaults.compression)),
      defaults.compression);
  config.module_filter =
      ReadEnvString("SENTRIFACE_LOG_MODULES", defaults.module_filter);
  config.file_path =
      ReadEnvString("SENTRIFACE_LOG_FILE", defaults.file_path);
  config.rpc_endpoint =
      ReadEnvString("SENTRIFACE_LOG_RPC_ENDPOINT", defaults.rpc_endpoint);
  return config;
}

const char* LogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::kOff:
      return "off";
    case LogLevel::kError:
      return "error";
    case LogLevel::kWarn:
      return "warn";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kDebug:
      return "debug";
  }
  return "off";
}

const char* LogBackendName(LogBackend backend) {
  switch (backend) {
    case LogBackend::kDummy:
      return "dummy";
    case LogBackend::kStdout:
      return "stdout";
    case LogBackend::kFile:
      return "file";
    case LogBackend::kFileBinary:
      return "file_binary";
    case LogBackend::kNetworkRpc:
      return "network_rpc";
  }
  return "dummy";
}

const char* LogCompressionName(LogCompression compression) {
  switch (compression) {
    case LogCompression::kNone:
      return "none";
    case LogCompression::kGzip:
      return "gzip";
    case LogCompression::kZstd:
      return "zstd";
  }
  return "none";
}

std::uint32_t CurrentTimestampMs() {
  using Clock = std::chrono::steady_clock;
  static const Clock::time_point start = Clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      Clock::now() - start);
  return static_cast<std::uint32_t>(elapsed.count());
}

}  // namespace sentriface::logging
