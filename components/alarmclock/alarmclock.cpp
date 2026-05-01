// Alarm clock component implementation.
// This file is only compiled as part of the ESPHome build (not host-side tests).

#include "alarmclock.h"

#ifndef UNIT_TEST

#include "esphome/core/log.h"

namespace alarmclock {

static const char *const TAG = "alarmclock";

void AlarmClockComponent::setup() {
  ESP_LOGI(TAG, "AlarmClock component initializing...");

  // Turn on backlight at maximum brightness.
  // STC8H1K28 I2C backlight controller: 0 = max, 244 = min, 245 = off.
  uint8_t brightness = kBacklightMax;
  if (this->write(&brightness, 1) != esphome::i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set backlight via I2C (address 0x%02X)",
             this->address_);
  } else {
    ESP_LOGI(TAG, "Backlight set to maximum brightness");
  }
}

void AlarmClockComponent::loop() {
  // TODO: Check current time against alarm schedule.
  // TODO: Read ambient light sensor and update backlight.
  // TODO: Handle snooze timer countdown.
}

}  // namespace alarmclock

#endif  // UNIT_TEST
