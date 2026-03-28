#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#define SENTRIFACE_LOG_TAG "tracker"
#include "logging/system_logger.hpp"

int main() {
  using sentriface::logging::LoadLoggerConfigFromEnv;
  using sentriface::logging::LogBackend;
  using sentriface::logging::LogLevel;
  using sentriface::logging::LogMessage;
  using sentriface::logging::SystemLogger;

  const std::string log_path = "system_logger_test.log";
  const std::string binary_log_path = "system_logger_test.flog";
  const std::string compressed_binary_log_path = "system_logger_test.flog.zst";
  const std::string decompressed_binary_log_path = "system_logger_test_decompressed.flog";
  std::remove(log_path.c_str());
  std::remove(binary_log_path.c_str());
  std::remove(compressed_binary_log_path.c_str());
  std::remove(decompressed_binary_log_path.c_str());

  sentriface::logging::LoggerConfig config;
  config.enabled = true;
  config.level = LogLevel::kInfo;
  config.backend = LogBackend::kFile;
  config.module_filter = "tracker,decision";
  config.file_path = log_path;
  SystemLogger logger(config);
  if (!logger.IsReady()) {
    return 1;
  }

  LogMessage message;
  message.level = LogLevel::kInfo;
  message.timestamp_ms = 1234U;
  message.module = "tracker";
  message.message = "hello";
  const auto result = logger.Log(message);
  if (!result.ok || !result.emitted) {
    return 2;
  }

  std::ifstream in(log_path);
  if (!in.is_open()) {
    return 3;
  }
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  if (content.find("+1234ms I/tracker: hello") == std::string::npos) {
    return 4;
  }

  const auto auto_result = logger.LogNow(LogLevel::kInfo, "tracker", "auto_message");
  if (!auto_result.ok || !auto_result.emitted) {
    return 5;
  }

  SENTRIFACE_LOGI(logger, "macro_message");

  message.module = "camera";
  const auto filtered = logger.Log(message);
  if (!filtered.ok || filtered.emitted) {
    return 6;
  }

  in.close();
  in.open(log_path);
  if (!in.is_open()) {
    return 7;
  }
  content.assign((std::istreambuf_iterator<char>(in)),
                 std::istreambuf_iterator<char>());
  if (content.find("I/tracker: auto_message") == std::string::npos ||
      content.find("I/tracker: macro_message") == std::string::npos) {
    return 8;
  }

  std::remove(log_path.c_str());

  sentriface::logging::LoggerConfig binary_config;
  binary_config.enabled = true;
  binary_config.level = LogLevel::kDebug;
  binary_config.backend = LogBackend::kFileBinary;
  binary_config.file_path = binary_log_path;
  SystemLogger binary_logger(binary_config);
  if (!binary_logger.IsReady()) {
    return 9;
  }
  const auto binary_result =
      binary_logger.LogNow(LogLevel::kInfo, "tracker", "binary_message");
  if (!binary_result.ok || !binary_result.emitted) {
    return 10;
  }
  std::ifstream binary_in(binary_log_path, std::ios::binary);
  if (!binary_in.is_open()) {
    return 11;
  }
  char magic[4] = {};
  binary_in.read(magic, 4);
  if (binary_in.gcount() != 4 || std::string(magic, 4) != "FLOG") {
    return 12;
  }
  std::string binary_content((std::istreambuf_iterator<char>(binary_in)),
                             std::istreambuf_iterator<char>());
  if (binary_content.find("tracker") == std::string::npos ||
      binary_content.find("binary_message") == std::string::npos) {
    return 13;
  }
  std::remove(binary_log_path.c_str());

  sentriface::logging::LoggerConfig compressed_binary_config;
  compressed_binary_config.enabled = true;
  compressed_binary_config.level = LogLevel::kDebug;
  compressed_binary_config.backend = LogBackend::kFileBinary;
  compressed_binary_config.compression = sentriface::logging::LogCompression::kZstd;
  compressed_binary_config.file_path = compressed_binary_log_path;
  SystemLogger compressed_binary_logger(compressed_binary_config);
  if (!compressed_binary_logger.IsReady()) {
    return 14;
  }
  const auto compressed_binary_result =
      compressed_binary_logger.LogNow(LogLevel::kInfo, "tracker", "compressed_binary_message");
  if (!compressed_binary_result.ok || !compressed_binary_result.emitted) {
    return 15;
  }
  {
    std::ifstream compressed_in(compressed_binary_log_path, std::ios::binary);
    if (!compressed_in.is_open()) {
      return 16;
    }
  }
  {
    std::string command =
        "zstd -q -d -c " + compressed_binary_log_path + " > " + decompressed_binary_log_path;
    if (std::system(command.c_str()) != 0) {
      return 17;
    }
  }
  std::ifstream decompressed_in(decompressed_binary_log_path, std::ios::binary);
  if (!decompressed_in.is_open()) {
    return 18;
  }
  char compressed_magic[4] = {};
  decompressed_in.read(compressed_magic, 4);
  if (decompressed_in.gcount() != 4 || std::string(compressed_magic, 4) != "FLOG") {
    return 19;
  }
  std::string decompressed_content((std::istreambuf_iterator<char>(decompressed_in)),
                                   std::istreambuf_iterator<char>());
  if (decompressed_content.find("tracker") == std::string::npos ||
      decompressed_content.find("compressed_binary_message") == std::string::npos) {
    return 20;
  }
  std::remove(compressed_binary_log_path.c_str());
  std::remove(decompressed_binary_log_path.c_str());

  setenv("SENTRIFACE_LOG_ENABLE", "1", 1);
  setenv("SENTRIFACE_LOG_LEVEL", "debug", 1);
  setenv("SENTRIFACE_LOG_BACKEND", "network_rpc", 1);
  setenv("SENTRIFACE_LOG_COMPRESSION", "zstd", 1);
  setenv("SENTRIFACE_LOG_MODULES", "tracker,decision", 1);
  setenv("SENTRIFACE_LOG_RPC_ENDPOINT", "tcp://127.0.0.1:9000", 1);
  const auto env_config = LoadLoggerConfigFromEnv();
  if (!env_config.enabled ||
      env_config.level != LogLevel::kDebug ||
      env_config.backend != LogBackend::kNetworkRpc ||
      env_config.compression != sentriface::logging::LogCompression::kZstd ||
      env_config.module_filter != "tracker,decision" ||
      env_config.rpc_endpoint != "tcp://127.0.0.1:9000") {
    return 21;
  }

  SystemLogger rpc_logger(env_config);
  if (!rpc_logger.IsReady()) {
    return 22;
  }
  message.level = LogLevel::kDebug;
  message.module = "decision";
  const auto rpc_result = rpc_logger.Log(message);
  if (!rpc_result.ok || !rpc_result.emitted) {
    return 23;
  }

  return 0;
}
