#ifndef SENTRIFACE_ACCESS_UNLOCK_CONTROLLER_HPP_
#define SENTRIFACE_ACCESS_UNLOCK_CONTROLLER_HPP_

#include <cstdint>
#include <string>

#include "logging/system_logger.hpp"

namespace sentriface::access {

enum class UnlockBackend : std::uint8_t {
  kDummy = 0,
  kStdout = 1,
  kLibgpiod = 2,
  kUart = 3,
  kNetworkRpc = 4,
};

struct UnlockControllerConfig {
  UnlockBackend backend = UnlockBackend::kDummy;
  int line_offset = -1;
  int uart_baud_rate = 115200;
  int pulse_ms = 500;
  int cooldown_ms = 2000;
  bool active_low = false;
  std::string chip_path;
  std::string line_name;
  std::string uart_device;
  std::string rpc_endpoint;
  sentriface::logging::LoggerConfig logger_config {};
};

struct UnlockEvent {
  int track_id = -1;
  int person_id = -1;
  std::uint32_t timestamp_ms = 0;
  std::string label;
};

struct UnlockResult {
  bool ok = false;
  bool fired = false;
  std::string message;
};

class UnlockController {
 public:
  explicit UnlockController(
      const UnlockControllerConfig& config = UnlockControllerConfig {});

  bool IsReady() const;
  UnlockResult TriggerUnlock(const UnlockEvent& event);

  UnlockBackend backend() const;
  int cooldown_ms() const;

 private:
  UnlockResult TriggerDummy(const UnlockEvent& event);
  UnlockResult TriggerStdout(const UnlockEvent& event);
  UnlockResult TriggerLibgpiod(const UnlockEvent& event);
  UnlockResult TriggerUart(const UnlockEvent& event);
  UnlockResult TriggerNetworkRpc(const UnlockEvent& event);

  UnlockControllerConfig config_;
  sentriface::logging::SystemLogger logger_;
  std::uint32_t last_unlock_timestamp_ms_ = 0;
  bool has_fired_once_ = false;
};

UnlockControllerConfig LoadUnlockControllerConfigFromEnv(
    const UnlockControllerConfig& defaults = UnlockControllerConfig {});

const char* UnlockBackendName(UnlockBackend backend);

}  // namespace sentriface::access

#endif  // SENTRIFACE_ACCESS_UNLOCK_CONTROLLER_HPP_
