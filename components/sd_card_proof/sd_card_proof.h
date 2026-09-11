#pragma once

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

 protected:
  int clk_pin_{-1};
  int cmd_pin_{-1};
  int data0_pin_{-1};
  sdmmc_card_t *card_{nullptr};
};

}  // namespace sd_card_proof
}  // namespace esphome
