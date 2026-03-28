#define SENTRIFACE_LOG_TAG "access"
#include "access/unlock_controller.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>

namespace sentriface::access {

namespace {

int ReadEnvInt(const char* name, int default_value) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return default_value;
  }
  return std::atoi(value);
}

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

UnlockBackend ParseBackend(const std::string& value, UnlockBackend fallback) {
  if (value == "dummy") {
    return UnlockBackend::kDummy;
  }
  if (value == "stdout") {
    return UnlockBackend::kStdout;
  }
  if (value == "gpiod" || value == "libgpiod") {
    return UnlockBackend::kLibgpiod;
  }
  if (value == "uart") {
    return UnlockBackend::kUart;
  }
  if (value == "rpc" || value == "network" || value == "network_rpc") {
    return UnlockBackend::kNetworkRpc;
  }
  return fallback;
}

std::string ReadBackendEnvName(const UnlockControllerConfig& defaults) {
  const std::string unlock_name =
      ReadEnvString("SENTRIFACE_UNLOCK_BACKEND", std::string());
  if (!unlock_name.empty()) {
    return unlock_name;
  }
  return ReadEnvString("SENTRIFACE_GPIO_BACKEND", UnlockBackendName(defaults.backend));
}

}  // namespace

UnlockController::UnlockController(const UnlockControllerConfig& config)
    : config_(config), logger_(config.logger_config) {}

bool UnlockController::IsReady() const {
  switch (config_.backend) {
    case UnlockBackend::kDummy:
    case UnlockBackend::kStdout:
      return true;
    case UnlockBackend::kLibgpiod:
#ifdef SENTRIFACE_HAS_GPIOD
      return !config_.chip_path.empty() && config_.line_offset >= 0;
#else
      return false;
#endif
    case UnlockBackend::kUart:
      return !config_.uart_device.empty();
    case UnlockBackend::kNetworkRpc:
      return !config_.rpc_endpoint.empty();
  }
  return false;
}

UnlockResult UnlockController::TriggerUnlock(const UnlockEvent& event) {
  if (has_fired_once_) {
    const std::uint32_t elapsed = event.timestamp_ms - last_unlock_timestamp_ms_;
    if (elapsed < static_cast<std::uint32_t>(config_.cooldown_ms)) {
      UnlockResult result;
      result.ok = true;
      result.fired = false;
      result.message = "cooldown_active";
      std::ostringstream oss;
      oss << "unlock_cooldown_active"
          << " track_id=" << event.track_id
          << " person_id=" << event.person_id
          << " label=" << event.label
          << " elapsed_ms=" << elapsed;
      SENTRIFACE_LOGD(logger_, oss.str());
      return result;
    }
  }

  UnlockResult result;
  switch (config_.backend) {
    case UnlockBackend::kDummy:
      result = TriggerDummy(event);
      break;
    case UnlockBackend::kStdout:
      result = TriggerStdout(event);
      break;
    case UnlockBackend::kLibgpiod:
      result = TriggerLibgpiod(event);
      break;
    case UnlockBackend::kUart:
      result = TriggerUart(event);
      break;
    case UnlockBackend::kNetworkRpc:
      result = TriggerNetworkRpc(event);
      break;
  }

  if (result.ok && result.fired) {
    last_unlock_timestamp_ms_ = event.timestamp_ms;
    has_fired_once_ = true;
  }

  std::ostringstream oss;
  oss << "unlock_result"
      << " backend=" << UnlockBackendName(config_.backend)
      << " track_id=" << event.track_id
      << " person_id=" << event.person_id
      << " label=" << event.label
      << " ok=" << (result.ok ? 1 : 0)
      << " fired=" << (result.fired ? 1 : 0)
      << " detail=" << result.message;
  if (result.ok) {
    SENTRIFACE_LOGI(logger_, oss.str());
  } else {
    SENTRIFACE_LOGW(logger_, oss.str());
  }
  return result;
}

UnlockBackend UnlockController::backend() const { return config_.backend; }

int UnlockController::cooldown_ms() const { return config_.cooldown_ms; }

UnlockResult UnlockController::TriggerDummy(const UnlockEvent&) {
  UnlockResult result;
  result.ok = true;
  result.fired = true;
  result.message = "dummy_unlock";
  return result;
}

UnlockResult UnlockController::TriggerStdout(const UnlockEvent& event) {
  std::ostringstream oss;
  oss << "gpio_unlock backend=stdout"
      << " ts=" << event.timestamp_ms
      << " track_id=" << event.track_id
      << " person_id=" << event.person_id
      << " label=" << event.label
      << " pulse_ms=" << config_.pulse_ms;
  std::cout << oss.str() << '\n';

  UnlockResult result;
  result.ok = true;
  result.fired = true;
  result.message = oss.str();
  return result;
}

UnlockResult UnlockController::TriggerLibgpiod(const UnlockEvent&) {
  UnlockResult result;
#ifdef SENTRIFACE_HAS_GPIOD
  result.ok = false;
  result.fired = false;
  result.message = "libgpiod_backend_not_implemented_yet";
#else
  result.ok = false;
  result.fired = false;
  result.message = "libgpiod_backend_not_available";
#endif
  return result;
}

UnlockResult UnlockController::TriggerUart(const UnlockEvent& event) {
  std::ostringstream oss;
  oss << "unlock_backend=uart"
      << " ts=" << event.timestamp_ms
      << " track_id=" << event.track_id
      << " person_id=" << event.person_id
      << " label=" << event.label
      << " device=" << config_.uart_device
      << " baud=" << config_.uart_baud_rate
      << " pulse_ms=" << config_.pulse_ms;
  UnlockResult result;
  result.ok = !config_.uart_device.empty();
  result.fired = result.ok;
  result.message = result.ok ? oss.str() : "uart_backend_not_configured";
  return result;
}

UnlockResult UnlockController::TriggerNetworkRpc(const UnlockEvent& event) {
  std::ostringstream oss;
  oss << "unlock_backend=network_rpc"
      << " ts=" << event.timestamp_ms
      << " track_id=" << event.track_id
      << " person_id=" << event.person_id
      << " label=" << event.label
      << " endpoint=" << config_.rpc_endpoint
      << " pulse_ms=" << config_.pulse_ms;
  UnlockResult result;
  result.ok = !config_.rpc_endpoint.empty();
  result.fired = result.ok;
  result.message = result.ok ? oss.str() : "network_rpc_backend_not_configured";
  return result;
}

UnlockControllerConfig LoadUnlockControllerConfigFromEnv(
    const UnlockControllerConfig& defaults) {
  UnlockControllerConfig config = defaults;
  config.backend = ParseBackend(
      ReadBackendEnvName(defaults),
      defaults.backend);
  config.line_offset = ReadEnvInt(
      "SENTRIFACE_GPIO_LINE_OFFSET",
      ReadEnvInt("SENTRIFACE_UNLOCK_LINE_OFFSET", defaults.line_offset));
  config.pulse_ms = ReadEnvInt(
      "SENTRIFACE_UNLOCK_PULSE_MS",
      ReadEnvInt("SENTRIFACE_GPIO_PULSE_MS", defaults.pulse_ms));
  config.cooldown_ms = ReadEnvInt(
      "SENTRIFACE_UNLOCK_COOLDOWN_MS",
      ReadEnvInt("SENTRIFACE_GPIO_COOLDOWN_MS", defaults.cooldown_ms));
  config.active_low = ReadEnvBool(
      "SENTRIFACE_GPIO_ACTIVE_LOW",
      ReadEnvBool("SENTRIFACE_UNLOCK_ACTIVE_LOW", defaults.active_low));
  config.chip_path = ReadEnvString(
      "SENTRIFACE_GPIO_CHIP_PATH",
      ReadEnvString("SENTRIFACE_UNLOCK_CHIP_PATH", defaults.chip_path));
  config.line_name = ReadEnvString(
      "SENTRIFACE_GPIO_LINE_NAME",
      ReadEnvString("SENTRIFACE_UNLOCK_LINE_NAME", defaults.line_name));
  config.uart_device = ReadEnvString("SENTRIFACE_UNLOCK_UART_DEVICE", defaults.uart_device);
  config.uart_baud_rate = ReadEnvInt(
      "SENTRIFACE_UNLOCK_UART_BAUD",
      defaults.uart_baud_rate);
  config.rpc_endpoint = ReadEnvString(
      "SENTRIFACE_UNLOCK_RPC_ENDPOINT", defaults.rpc_endpoint);
  sentriface::logging::LoggerConfig logger_defaults = defaults.logger_config;
  if (logger_defaults.module_filter.empty()) {
    logger_defaults.module_filter = "access";
  }
  config.logger_config = sentriface::logging::LoadLoggerConfigFromEnv(logger_defaults);
  return config;
}

const char* UnlockBackendName(UnlockBackend backend) {
  switch (backend) {
    case UnlockBackend::kDummy:
      return "dummy";
    case UnlockBackend::kStdout:
      return "stdout";
    case UnlockBackend::kLibgpiod:
      return "gpiod";
    case UnlockBackend::kUart:
      return "uart";
    case UnlockBackend::kNetworkRpc:
      return "network_rpc";
  }
  return "dummy";
}

}  // namespace sentriface::access
