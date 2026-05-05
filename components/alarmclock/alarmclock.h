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
#include "ui.h"
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

// ---------------------------------------------------------------------------
// Brightness control constants.
// ---------------------------------------------------------------------------

// Default width of the auto-dimming window (0.0–1.0 scale).
// A value of 0.5 means the sensor can vary brightness by 50 percentage points.
static constexpr float kDefaultAutoRange = 0.5f;

// Absolute brightness floor / ceiling (0.0–1.0 scale).
static constexpr float kMinBrightness = 0.0f;
static constexpr float kMaxBrightness = 1.0f;

// Default user brightness level (midpoint).
static constexpr float kDefaultUserBrightness = 0.5f;

// Step size for manual brightness +/- buttons.
static constexpr float kBrightnessStep = 0.1f;

// ---------------------------------------------------------------------------
// Brightness pure functions.
// ---------------------------------------------------------------------------

// Compute backlight PWM value from ambient lux reading (legacy).
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

// Convert an ambient lux reading to a sensor factor (0.0–1.0).
//   - lux <= min_lux → 0.0 (dark)
//   - lux >= max_lux → 1.0 (bright)
inline float lux_to_sensor_factor(float lux, float min_lux, float max_lux) {
  if (max_lux <= min_lux) {
    return 0.0f;
  }
  if (lux <= min_lux) {
    return 0.0f;
  }
  if (lux >= max_lux) {
    return 1.0f;
  }
  return (lux - min_lux) / (max_lux - min_lux);
}

// Compute final brightness (0.0–1.0) from a user-chosen level and a sensor
// factor.  The user level slides an auto-dimming window across the full
// brightness range; the sensor factor picks a point within that window.
//
//   user_level:     0.0 (prefer dim) … 1.0 (prefer bright)
//   sensor_factor:  0.0 (dark room)  … 1.0 (bright room)
//   auto_range:     width of the sensor-controlled window (0.0–1.0)
//   min_brightness: absolute floor
//   max_brightness: absolute ceiling
//
// Examples with defaults (auto_range=0.5, min=0.1, max=1.0):
//   user_level=1.0 → window [0.50, 1.00], sensor picks within
//   user_level=0.0 → window [0.10, 0.60], sensor picks within
//   user_level=0.5 → window [0.30, 0.80]
//
// Without a sensor, pass sensor_factor=1.0 to get the window's upper bound.
inline float compute_brightness(float user_level, float sensor_factor,
                                float auto_range = kDefaultAutoRange,
                                float min_brightness = kMinBrightness,
                                float max_brightness = kMaxBrightness) {
  // Clamp inputs.
  if (user_level < 0.0f) { user_level = 0.0f; }
  if (user_level > 1.0f) { user_level = 1.0f; }
  if (sensor_factor < 0.0f) { sensor_factor = 0.0f; }
  if (sensor_factor > 1.0f) { sensor_factor = 1.0f; }
  if (max_brightness <= min_brightness) {
    return min_brightness;
  }

  float total = max_brightness - min_brightness;
  if (auto_range < 0.0f) { auto_range = 0.0f; }
  if (auto_range > total) { auto_range = total; }

  // The window slides from [min, min+auto_range] to [max-auto_range, max].
  float slide = total - auto_range;
  float range_min = min_brightness + user_level * slide;

  return range_min + sensor_factor * auto_range;
}

// Convert a brightness value (0.0–1.0) to the hardware PWM byte.
//   0.0 → kBacklightMin (244, dimmest)
//   1.0 → kBacklightMax (0, brightest)
inline uint8_t brightness_to_pwm(float brightness) {
  if (brightness <= 0.0f) {
    return kBacklightMin;
  }
  if (brightness >= 1.0f) {
    return kBacklightMax;
  }
  return static_cast<uint8_t>(
      static_cast<float>(kBacklightMin) * (1.0f - brightness));
}

// Compute a grayscale content color (0x000000–0xFFFFFF) to supplement backlight
// dimming.  At full brightness the color is white; at low brightness the color
// fades toward dark gray.  This extends the perceived dynamic range beyond what
// the backlight PWM alone can achieve.
//
// The mapping uses a power curve so that the color stays mostly white in the
// upper half and falls off more aggressively in the lower half:
//   brightness 1.0 → 0xFF (white)
//   brightness 0.5 → ~0xB4
//   brightness 0.0 → 0x1A (very dark gray, not pure black)
inline uint32_t compute_content_color(float brightness) {
  if (brightness >= 1.0f) {
    return 0xFFFFFF;
  }
  if (brightness <= 0.0f) {
    return 0x1A1A1A;
  }
  // Map brightness (0–1) to a channel value (26–255).
  // Use sqrt curve so color stays bright longer and drops at low end.
  float t = brightness * brightness;  // quadratic falloff
  uint8_t ch = static_cast<uint8_t>(26.0f + t * (255.0f - 26.0f));
  return (static_cast<uint32_t>(ch) << 16) |
         (static_cast<uint32_t>(ch) << 8) |
         static_cast<uint32_t>(ch);
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

// Maximum number of configurable alarms.
static constexpr uint8_t kMaxAlarms = 4;

class AlarmClockComponent : public esphome::Component,
                            public esphome::i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override {
    return esphome::setup_priority::HARDWARE;
  }

  // --- Alarm management (called from HA or UI) ---
  void set_alarm(uint8_t index, uint8_t hour, uint8_t minute,
                 uint8_t days_mask, bool enabled);
  void enable_alarm(uint8_t index, bool enabled);
  void dismiss_alarm();
  void snooze_alarm();

  // --- Settings ---
  void set_volume(float volume);
  void set_brightness(float brightness);
  float volume() const { return volume_; }
  float brightness() const { return brightness_; }

  // --- State queries (for HA sensors) ---
  alarm_clock::AlarmState alarm_state() const { return state_machine_.state(); }
  bool is_firing() const {
    return state_machine_.state() == alarm_clock::AlarmState::kFiring;
  }

  // Called from YAML interval to check alarms against current time.
  void check_alarms_(uint8_t hour, uint8_t minute, uint8_t day_of_week);

 private:
  // Alarm storage.
  alarm_clock::AlarmTime alarms_[kMaxAlarms] = {};
  alarm_clock::AlarmStateMachine state_machine_;

  // Settings.
  float volume_ = 0.5f;
  float brightness_ = 0.5f;
  float sensor_factor_ = 1.0f;

  // Timing.
  uint32_t last_minute_check_ms_ = 0;
  uint8_t last_checked_minute_ = 0xFF;

  // Internal helpers.
  void update_backlight_();
  void start_alarm_sound_();
  void stop_alarm_sound_();
  void sync_ui_();
};

#endif  // UNIT_TEST

}  // namespace alarmclock

#endif  // ALARMCLOCK_H
