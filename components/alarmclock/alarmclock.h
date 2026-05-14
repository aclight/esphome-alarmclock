// Alarm clock component header.
// Contains constants, testable pure functions, and the AlarmClockComponent class.
// Pure functions compile standalone under -DUNIT_TEST without ESPHome dependencies.
#ifndef ALARMCLOCK_H
#define ALARMCLOCK_H

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>

#include "alarm_time.h"
#include "alarm_state.h"
#include "storage.h"

// ESPHome headers must be included outside the alarmclock namespace.
#ifndef UNIT_TEST
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "ui.h"
#endif  // UNIT_TEST

namespace alarmclock {

// ---------------------------------------------------------------------------
// Audio playback constants.
// ---------------------------------------------------------------------------

// Duration of the volume ramp-up when alarm starts (milliseconds).
static constexpr uint32_t kVolumeRampDurationMs = 30000;

// Starting fraction of configured volume at ramp start.
static constexpr float kVolumeRampStartFraction = 0.1f;

// Pause duration between RTTTL melody loops (milliseconds).
static constexpr uint32_t kAlarmPauseDurationMs = 2500;

// Default RTTTL alarm melody (classic beep pattern).
static constexpr const char *kDefaultAlarmMelody =
    "Alarm:d=8,o=6,b=400:c,p,c,p,c,4p,c,p,c,p,c";

// ---------------------------------------------------------------------------
// Alarm sound definitions — a menu of built-in RTTTL melodies.
// ---------------------------------------------------------------------------

struct AlarmSound {
  const char *name;
  const char *rtttl;
};

static constexpr uint8_t kAlarmSoundCount = 5;

static const AlarmSound kAlarmSounds[kAlarmSoundCount] = {
    {"Classic Beep", "Alarm:d=8,o=6,b=400:c,p,c,p,c,4p,c,p,c,p,c"},
    {"Morning Rise", "Morning:d=4,o=5,b=160:c,e,g,8a,8g,e,c,2p,c,e,g,a"},
    {"Gentle Chime", "Chime:d=8,o=6,b=200:e,g,a,g,e,4p,e,g,a,g,e"},
    {"Digital Buzz", "Buzz:d=16,o=7,b=600:c,p,c,p,c,p,c,8p,c,p,c,p,c,p,c"},
    {"Melody Wake", "Wake:d=8,o=5,b=180:g,a,b,d6,4b,a,g,4a,2g"},
};

// Get the RTTTL string for a sound index (clamped to valid range).
inline const char *get_alarm_sound_rtttl(uint8_t index) {
  if (index >= kAlarmSoundCount) {
    index = 0;
  }
  return kAlarmSounds[index].rtttl;
}

// Get the name for a sound index (clamped to valid range).
inline const char *get_alarm_sound_name(uint8_t index) {
  if (index >= kAlarmSoundCount) {
    index = 0;
  }
  return kAlarmSounds[index].name;
}

// ---------------------------------------------------------------------------
// Snooze duration options.
// ---------------------------------------------------------------------------

static constexpr uint8_t kSnoozeDurationOptionCount = 4;
static constexpr uint8_t kSnoozeDurationOptions[kSnoozeDurationOptionCount] = {
    5, 9, 10, 15};

// Convert a snooze option index (0–3) to duration in minutes.
// Returns the default (9 min) if index is out of range.
inline uint8_t snooze_option_to_minutes(uint8_t option_index) {
  if (option_index >= kSnoozeDurationOptionCount) {
    return kSnoozeDurationOptions[1];  // default: 9 min
  }
  return kSnoozeDurationOptions[option_index];
}

// Find the option index for a given snooze duration in minutes.
// Returns the index, or 1 (default = 9 min) if not found.
inline uint8_t snooze_minutes_to_option(uint8_t minutes) {
  for (uint8_t i = 0; i < kSnoozeDurationOptionCount; ++i) {
    if (kSnoozeDurationOptions[i] == minutes) {
      return i;
    }
  }
  return 1;  // default: index 1 = 9 min
}

// ---------------------------------------------------------------------------
// Volume ramp pure function — testable on the host without ESPHome.
// ---------------------------------------------------------------------------

// Compute the ramped volume at a given elapsed time.
//   configured_volume: target volume (0.0–1.0)
//   elapsed_ms:        time since alarm started sounding
//   ramp_duration_ms:  total ramp-up duration
// Returns: current volume, linearly ramped from
//   (kVolumeRampStartFraction * configured_volume) at t=0
//   to configured_volume at t=ramp_duration_ms.
inline float compute_ramp_volume(float configured_volume, uint32_t elapsed_ms,
                                 uint32_t ramp_duration_ms = kVolumeRampDurationMs) {
  if (configured_volume <= 0.0f) {
    return 0.0f;
  }
  if (ramp_duration_ms == 0 || elapsed_ms >= ramp_duration_ms) {
    return configured_volume;
  }
  float t = static_cast<float>(elapsed_ms) / static_cast<float>(ramp_duration_ms);
  return configured_volume * (kVolumeRampStartFraction + (1.0f - kVolumeRampStartFraction) * t);
}

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
// Screen sleep / wake constants.
// ---------------------------------------------------------------------------

// Idle timeout before dimming the screen (milliseconds).
static constexpr uint32_t kScreenIdleTimeoutMs = 30000;

// When asleep, use this user_level to compute a dim backlight.
// This results in a sensor-based minimum brightness.
static constexpr float kSleepUserLevel = 0.0f;

// Minimum delay between NVS settings writes (milliseconds).
// Prevents flash wear from continuous slider drag events.
static constexpr uint32_t kSettingsDebouncePeriodMs = 2000;

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
// Examples with defaults (auto_range=0.5, min=0.0, max=1.0):
//   user_level=1.0 → window [0.50, 1.00], sensor picks within
//   user_level=0.0 → window [0.00, 0.50], sensor picks within
//   user_level=0.5 → window [0.25, 0.75]
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

// ---------------------------------------------------------------------------
// 12-hour / 24-hour time conversion helpers.
// ---------------------------------------------------------------------------

// Convert a 24-hour hour (0–23) to 12-hour format.
// Returns {display_hour (1–12), is_pm}.
inline std::pair<uint8_t, bool> hour_24_to_12(uint8_t hour_24) {
  bool is_pm = (hour_24 >= 12);
  uint8_t h12 = hour_24 % 12;
  if (h12 == 0) {
    h12 = 12;
  }
  return {h12, is_pm};
}

// Convert a 12-hour hour (1–12) + AM/PM flag to 24-hour format (0–23).
inline uint8_t hour_12_to_24(uint8_t hour_12, bool is_pm) {
  if (hour_12 == 12) {
    return is_pm ? 12 : 0;
  }
  return is_pm ? (hour_12 + 12) : hour_12;
}

// ---------------------------------------------------------------------------
// Time format helper — format a clock time string.
// ---------------------------------------------------------------------------

// Format a clock time into a buffer.
// In 12-hour mode: "7:00 AM", in 24-hour mode: "07:00".
// Returns the number of characters written (excluding null terminator).
inline size_t format_clock_time(uint8_t hour, uint8_t minute,
                                bool time_format_24h, char *buf,
                                size_t buf_size) {
  if (buf == nullptr || buf_size == 0) {
    return 0;
  }
  int written;
  if (time_format_24h) {
    written = snprintf(buf, buf_size, "%02d:%02d", hour, minute);
  } else {
    auto [h12, is_pm] = hour_24_to_12(hour);
    const char *ampm = is_pm ? "PM" : "AM";
    written = snprintf(buf, buf_size, "%d:%02d %s", h12, minute, ampm);
  }
  if (written < 0) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(written);
}

// ---------------------------------------------------------------------------
// Pre-alarm notification threshold (minutes before alarm).
// ---------------------------------------------------------------------------
static constexpr uint8_t kDefaultPreAlarmMinutes = 5;
static constexpr uint8_t kPreAlarmOptionCount = 4;
static constexpr uint8_t kPreAlarmOptions[kPreAlarmOptionCount] = {0, 5, 10, 15};

// Convert a pre-alarm option index to minutes.
// Returns the default (5 min) if index is out of range.
inline uint8_t pre_alarm_option_to_minutes(uint8_t option_index) {
  if (option_index >= kPreAlarmOptionCount) {
    return kPreAlarmOptions[1];  // default: 5 min
  }
  return kPreAlarmOptions[option_index];
}

// Find the option index for a given pre-alarm duration in minutes.
// Returns the index, or 1 (default = 5 min) if not found.
inline uint8_t pre_alarm_minutes_to_option(uint8_t minutes) {
  for (uint8_t i = 0; i < kPreAlarmOptionCount; ++i) {
    if (kPreAlarmOptions[i] == minutes) {
      return i;
    }
  }
  return 1;  // default: index 1 = 5 min
}

// Maximum number of configurable alarms (used by pure functions too).
static constexpr uint8_t kMaxAlarmsCount = 4;

// ---------------------------------------------------------------------------
// Next alarm computation — finds nearest upcoming alarm.
// ---------------------------------------------------------------------------

// Find the index of the nearest upcoming alarm from an array.
// Returns the index (0-based), or -1 if no alarm is upcoming.
// |minutes_out| receives the minutes until that alarm (if not nullptr).
inline int8_t find_next_alarm_index(const AlarmTime *alarms,
                                    uint8_t count, uint8_t now_hour,
                                    uint8_t now_minute, uint8_t now_day_index,
                                    int32_t *minutes_out = nullptr) {
  int8_t best_index = -1;
  int32_t best_minutes = INT32_MAX;

  for (uint8_t i = 0; i < count; ++i) {
    int32_t mins = minutes_until_alarm(
        alarms[i], now_hour, now_minute, now_day_index);
    if (mins >= 0 && mins < best_minutes) {
      best_minutes = mins;
      best_index = static_cast<int8_t>(i);
    }
  }

  if (minutes_out != nullptr && best_index >= 0) {
    *minutes_out = best_minutes;
  }
  return best_index;
}

// Format the "next alarm" display string.
// Output example: "7:00 AM — Work (in 6h 30m)" or "07:00 — Work (in 6h 30m)"
// Returns the number of characters written (excluding null terminator).
inline size_t format_next_alarm_text(const AlarmTime &alarm,
                                     int32_t minutes_until,
                                     bool time_format_24h, char *buf,
                                     size_t buf_size) {
  if (buf == nullptr || buf_size == 0) {
    return 0;
  }

  // Format the time portion.
  char time_str[12];
  format_clock_time(alarm.hour, alarm.minute, time_format_24h, time_str,
                    sizeof(time_str));

  uint16_t hours = static_cast<uint16_t>(minutes_until / 60);
  uint16_t mins = static_cast<uint16_t>(minutes_until % 60);

  int written;
  if (alarm.label[0] != '\0') {
    if (hours > 0) {
      written = snprintf(buf, buf_size, "%s \xe2\x80\x94 %s (in %uh %um)",
                         time_str, alarm.label, hours, mins);
    } else {
      written = snprintf(buf, buf_size, "%s \xe2\x80\x94 %s (in %um)",
                         time_str, alarm.label, mins);
    }
  } else {
    if (hours > 0) {
      written = snprintf(buf, buf_size, "%s (in %uh %um)",
                         time_str, hours, mins);
    } else {
      written = snprintf(buf, buf_size, "%s (in %um)",
                         time_str, mins);
    }
  }
  if (written < 0) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(written);
}

// Format the pre-alarm banner text.
// Output example: "Alarm in 5 min \xe2\x80\x94 Work" or "Alarm in 5 min"
// Returns the number of characters written (excluding null terminator).
inline size_t format_pre_alarm_text(uint16_t minutes_remaining,
                                    const char *label, char *buf,
                                    size_t buf_size) {
  if (buf == nullptr || buf_size == 0) {
    return 0;
  }

  int written;
  if (label != nullptr && label[0] != '\0') {
    written = snprintf(buf, buf_size, "Alarm in %u min \xe2\x80\x94 %s",
                       minutes_remaining, label);
  } else {
    written = snprintf(buf, buf_size, "Alarm in %u min", minutes_remaining);
  }
  if (written < 0) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(written);
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

}  // namespace alarmclock

// Forward-declare RTTTL component at global scope.
namespace esphome {
namespace rtttl {
class Rtttl;
}  // namespace rtttl
}  // namespace esphome

namespace alarmclock {

// Maximum number of configurable alarms.
static constexpr uint8_t kMaxAlarms = kMaxAlarmsCount;

class AlarmClockComponent : public ::esphome::Component,
                            public ::esphome::i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override {
    return ::esphome::setup_priority::LATE;
  }

  // --- Alarm management (called from HA or UI) ---
  void set_alarm(uint8_t index, uint8_t hour, uint8_t minute,
                 uint8_t days_mask, bool enabled);
  void set_alarm(uint8_t index, uint8_t hour, uint8_t minute,
                 uint8_t days_mask, bool enabled, const char *label);
  void enable_alarm(uint8_t index, bool enabled);
  void update_alarm_time(uint8_t index, uint8_t hour, uint8_t minute);
  void update_alarm_days(uint8_t index, uint8_t days_mask);
  void update_alarm_label(uint8_t index, const char *label);
  void edit_alarm(uint8_t index);
  void delete_alarm(uint8_t index);
  void dismiss_alarm();
  void snooze_alarm();

  // --- Settings ---
  void set_volume(float volume);
  void set_brightness(float brightness);
  void set_sensor_factor(float sensor_factor);
  void set_sound_index(uint8_t index);
  void set_snooze_duration_option(uint8_t option_index);
  void set_time_format_24h(bool time_format_24h);
  void set_pre_alarm_option(uint8_t option_index);
  float volume() const { return volume_; }
  float brightness() const { return brightness_; }
  uint8_t sound_index() const { return selected_sound_index_; }
  bool time_format_24h() const { return time_format_24h_; }
  uint8_t pre_alarm_minutes() const { return pre_alarm_minutes_; }

  // --- RTTTL audio ---
  void set_rtttl(::esphome::rtttl::Rtttl *rtttl) { rtttl_ = rtttl; }
  void on_rtttl_finished();

  // --- Screen sleep/wake ---
  bool is_screen_asleep() const { return screen_asleep_; }

  // --- State queries (for HA sensors) ---
  AlarmState alarm_state() const { return state_machine_.state(); }
  bool is_firing() const {
    return state_machine_.state() == AlarmState::kFiring;
  }

  // Called from YAML interval to check alarms against current time.
  void check_alarms_(uint8_t hour, uint8_t minute, uint8_t day_of_week);

 private:
  // Alarm storage.
  AlarmTime alarms_[kMaxAlarms] = {};
  AlarmStateMachine state_machine_;

  // Settings.
  float volume_ = 0.5f;
  float brightness_ = 0.5f;
  float sensor_factor_ = 1.0f;
  uint8_t selected_sound_index_ = 0;
  bool time_format_24h_ = false;
  uint8_t pre_alarm_minutes_ = kDefaultPreAlarmMinutes;

  // RTTTL audio.
  ::esphome::rtttl::Rtttl *rtttl_ = nullptr;
  bool alarm_sound_active_ = false;
  bool alarm_pause_active_ = false;
  uint32_t alarm_sound_start_ms_ = 0;
  uint32_t alarm_pause_start_ms_ = 0;

  // Timing.
  uint32_t last_minute_check_ms_ = 0;
  uint8_t last_checked_minute_ = 0xFF;
  uint8_t last_known_hour_ = 0;
  uint8_t last_known_minute_ = 0;

  // Screen sleep state.
  bool screen_asleep_ = false;

  // NVS write debouncing.
  bool settings_dirty_ = false;
  uint32_t settings_dirty_ms_ = 0;

  // Internal helpers.
  void update_backlight_();
  void check_screen_sleep_();
  void start_alarm_sound_();
  void stop_alarm_sound_();
  void play_alarm_melody_();
  void sync_ui_();
  void fire_ha_event_(const char *event_type);
  void auto_disable_one_shot_alarm_();
  void save_alarms_to_storage_();
  void save_settings_to_storage_();
  void mark_settings_dirty_();
  void load_from_storage_();
  void update_next_alarm_display_(uint8_t hour, uint8_t minute,
                                  uint8_t day_of_week);

  // Index of the alarm that is currently firing (0xFF = none).
  uint8_t fired_alarm_index_ = 0xFF;

  // Bitmask of alarms queued while another is firing (bit i = alarm i pending).
  uint8_t pending_alarm_mask_ = 0;
};

#endif  // UNIT_TEST

}  // namespace alarmclock

#endif  // ALARMCLOCK_H
