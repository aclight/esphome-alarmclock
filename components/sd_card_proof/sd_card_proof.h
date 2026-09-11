#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"

#include "driver/sdmmc_types.h"

namespace esphome {
namespace sd_card_proof {

class SdCardProof : public Component {
 public:
  void set_clk_pin(const int pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(const int pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(const int pin) { this->data0_pin_ = pin; }

  void setup() override;
  void loop() override;

 protected:
  enum class ProofResult : uint8_t {
    kNotRun,
    kMountFailed,
    kOpenFailed,
    kReadSucceeded,
  };

  void log_result_() const;

  int clk_pin_{-1};
  int cmd_pin_{-1};
  int data0_pin_{-1};
  sdmmc_card_t *card_{nullptr};
  ProofResult result_{ProofResult::kNotRun};
  int mount_error_{0};
  uint64_t size_mb_{0};
  uint32_t frequency_khz_{0};
  uint32_t last_log_ms_{0};
  size_t bytes_read_{0};
  char contents_[129]{};
};

}  // namespace sd_card_proof
}  // namespace esphome
