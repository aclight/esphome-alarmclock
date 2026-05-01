// Alarm clock component header.
// Contains constants, testable pure functions, and the AlarmClockComponent class.
// Pure functions compile standalone under -DUNIT_TEST without ESPHome dependencies.
#ifndef ALARMCLOCK_H
#define ALARMCLOCK_H

#include <cstdint>
#include <utility>

#include "alarm_time.h"
#include "alarm_state.h"

// ESPHome headers must be included outside the alarmclock namespace.
#ifndef UNIT_TEST
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#endif  // UNIT_TEST

namespace alarmclock {

// ---------------------------------------------------------------------------
// I2C register / value constants for the backlight controller.
// ---------------------------------------------------------------------------
static constexpr uint8_t kBacklightMax = 0;    // PWM duty for maximum brightness.
static constexpr uint8_t kBacklightMin = 244;  // PWM duty for minimum brightness.
static constexpr uint8_t kBacklightOff = 245;  // Value that turns backlight off.
static constexpr uint8_t kBuzzerOn = 246;      // Command to activate buzzer.
static constexpr uint8_t kBuzzerOff = 247;     // Command to deactivate buzzer.

// ---------------------------------------------------------------------------
// Pure functions — testable on the host without ESPHome.
// ---------------------------------------------------------------------------

// Compute backlight PWM value from ambient lux reading.
// Maps |lux| linearly within [min_lux, max_lux] to [kBacklightMin, kBacklightMax].
//   - lux <= min_lux  → kBacklightMin (dimmest)
//   - lux >= max_lux  → kBacklightMax (brightest)
// The result is inverted because lower PWM duty = brighter on this hardware.
inline uint8_t compute_backlight_value(float lux, float min_lux, float max_lux) {
  if (max_lux <= min_lux) {
    return kBacklightMin;
  }
  if (lux <= min_lux) {
    return kBacklightMin;
  }
  if (lux >= max_lux) {
    return kBacklightMax;
  }
  // Linear interpolation: dim (244) at min_lux → bright (0) at max_lux.
  float ratio = (lux - min_lux) / (max_lux - min_lux);
  return static_cast<uint8_t>(
      static_cast<float>(kBacklightMin) * (1.0f - ratio));
}

// Check whether alarm time matches current time (simple hour/minute check).
inline bool alarm_matches(uint8_t alarm_h, uint8_t alarm_m, uint8_t now_h,
                          uint8_t now_m) {
  return alarm_h == now_h && alarm_m == now_m;
}

// Compute snooze target time given current time and snooze duration.
// Handles rollover past midnight (23:55 + 9 min → 00:04).
// Returns {hour, minute}.
inline std::pair<uint8_t, uint8_t> compute_snooze_time(uint8_t h, uint8_t m,
                                                        uint8_t snooze_minutes) {
  uint16_t total = static_cast<uint16_t>(h) * 60 + m + snooze_minutes;
  total %= (24 * 60);
  return {static_cast<uint8_t>(total / 60), static_cast<uint8_t>(total % 60)};
}

// ---------------------------------------------------------------------------
// ESPHome component class (excluded from host-side unit tests).
// ---------------------------------------------------------------------------
#ifndef UNIT_TEST

class AlarmClockComponent : public esphome::Component,
                            public esphome::i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override {
    return esphome::setup_priority::HARDWARE;
  }
};

#endif  // UNIT_TEST

}  // namespace alarmclock

#endif  // ALARMCLOCK_H
