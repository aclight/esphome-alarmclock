// Alarm clock component implementation.
// This file is only compiled as part of the ESPHome build (not host-side tests).

#include "alarmclock.h"

#ifndef UNIT_TEST

#include "esphome/core/log.h"

namespace alarmclock {

static const char *const TAG = "alarmclock";

void AlarmClockComponent::setup() {
  ESP_LOGI(TAG, "AlarmClock component initializing...");

  // TODO: Initialize backlight via I2C.
  // TODO: Read saved alarm config from flash / Home Assistant.
}

void AlarmClockComponent::loop() {
  // TODO: Check current time against alarm schedule.
  // TODO: Read ambient light sensor and update backlight.
  // TODO: Handle snooze timer countdown.
}

}  // namespace alarmclock

#endif  // UNIT_TEST
