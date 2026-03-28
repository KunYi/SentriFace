#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "access/unlock_controller.hpp"

int main() {
  using sentriface::access::LoadUnlockControllerConfigFromEnv;
  using sentriface::access::UnlockBackend;
  using sentriface::access::UnlockController;
  using sentriface::access::UnlockControllerConfig;
  using sentriface::access::UnlockEvent;
  using sentriface::logging::LogBackend;
  using sentriface::logging::LogLevel;

  UnlockControllerConfig config;
  config.backend = UnlockBackend::kDummy;
  config.cooldown_ms = 1000;
  config.logger_config.enabled = true;
  config.logger_config.level = LogLevel::kDebug;
  config.logger_config.backend = LogBackend::kFile;
  config.logger_config.file_path = "unlock_controller_test.log";
  config.logger_config.module_filter = "access";
  std::remove(config.logger_config.file_path.c_str());
  UnlockController dummy(config);
  if (!dummy.IsReady()) {
    return 1;
  }

  UnlockEvent event;
  event.track_id = 1;
  event.person_id = 10;
  event.timestamp_ms = 100;
  event.label = "alice";

  const auto result1 = dummy.TriggerUnlock(event);
  if (!result1.ok || !result1.fired) {
    return 2;
  }

  event.timestamp_ms = 200;
  const auto result2 = dummy.TriggerUnlock(event);
  if (!result2.ok || result2.fired) {
    return 3;
  }

  std::ifstream log_in(config.logger_config.file_path);
  if (!log_in.is_open()) {
    return 4;
  }
  std::string log_content((std::istreambuf_iterator<char>(log_in)),
                          std::istreambuf_iterator<char>());
  if (log_content.find("I/access: unlock_result") == std::string::npos ||
      log_content.find("D/access: unlock_cooldown_active") == std::string::npos) {
    return 5;
  }
  std::remove(config.logger_config.file_path.c_str());

  setenv("SENTRIFACE_GPIO_BACKEND", "stdout", 1);
  setenv("SENTRIFACE_GPIO_PULSE_MS", "700", 1);
  setenv("SENTRIFACE_GPIO_COOLDOWN_MS", "1500", 1);
  UnlockController env_controller(LoadUnlockControllerConfigFromEnv());
  if (!env_controller.IsReady()) {
    return 6;
  }
  if (env_controller.backend() != UnlockBackend::kStdout) {
    return 7;
  }
  if (env_controller.cooldown_ms() != 1500) {
    return 8;
  }

  event.timestamp_ms = 5000;
  const auto result3 = env_controller.TriggerUnlock(event);
  if (!result3.ok || !result3.fired) {
    return 9;
  }

  unsetenv("SENTRIFACE_GPIO_BACKEND");
  unsetenv("SENTRIFACE_GPIO_PULSE_MS");
  unsetenv("SENTRIFACE_GPIO_COOLDOWN_MS");

  setenv("SENTRIFACE_UNLOCK_BACKEND", "uart", 1);
  setenv("SENTRIFACE_UNLOCK_UART_DEVICE", "/dev/ttyS3", 1);
  setenv("SENTRIFACE_UNLOCK_UART_BAUD", "57600", 1);
  UnlockController uart_controller(LoadUnlockControllerConfigFromEnv());
  if (!uart_controller.IsReady()) {
    return 10;
  }
  if (uart_controller.backend() != UnlockBackend::kUart) {
    return 11;
  }

  event.timestamp_ms = 9000;
  const auto result4 = uart_controller.TriggerUnlock(event);
  if (!result4.ok || !result4.fired) {
    return 12;
  }

  unsetenv("SENTRIFACE_UNLOCK_BACKEND");
  unsetenv("SENTRIFACE_UNLOCK_UART_DEVICE");
  unsetenv("SENTRIFACE_UNLOCK_UART_BAUD");

  setenv("SENTRIFACE_UNLOCK_BACKEND", "network_rpc", 1);
  setenv("SENTRIFACE_UNLOCK_RPC_ENDPOINT", "tcp://192.168.1.50:9000", 1);
  UnlockController rpc_controller(LoadUnlockControllerConfigFromEnv());
  if (!rpc_controller.IsReady()) {
    return 13;
  }
  if (rpc_controller.backend() != UnlockBackend::kNetworkRpc) {
    return 14;
  }
  event.timestamp_ms = 12000;
  const auto result5 = rpc_controller.TriggerUnlock(event);
  if (!result5.ok || !result5.fired) {
    return 15;
  }

  unsetenv("SENTRIFACE_UNLOCK_BACKEND");
  unsetenv("SENTRIFACE_UNLOCK_RPC_ENDPOINT");
  return 0;
}
