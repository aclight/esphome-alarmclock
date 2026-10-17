// Alarm clock component header.
// Contains constants, testable pure functions, and the AlarmClockComponent class.
// Pure functions compile standalone under -DUNIT_TEST without ESPHome dependencies.
#ifndef ALARMCLOCK_H
#define ALARMCLOCK_H

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <array>
#include <map>
#include <string>
#include <utility>

#include "alarm_time.h"
#include "alarm_state.h"
#include "storage.h"
#include "esphome/components/adaptive_brightness/adaptive_brightness.h"

// ESPHome headers must be included outside the alarmclock namespace.
#ifndef UNIT_TEST
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/i2c/i2c.h"
#include "ui.h"
#endif  // UNIT_TEST

namespace alarmclock {

static_assert(adaptive_brightness::kKnotCount == kStorageCurveKnots,
              "storage.h kStorageCurveKnots must match the adaptive brightness curve");

// ---------------------------------------------------------------------------
// Audio playback constants.
// ---------------------------------------------------------------------------

// Duration of the volume ramp-up when alarm starts (milliseconds).
static constexpr uint32_t kVolumeRampDurationMs = 30000;

// Starting fraction of configured volume at ramp start.
static constexpr float kVolumeRampStartFraction = 0.1f;

// Pause duration between alarm sound loops (milliseconds).
static constexpr uint32_t kAlarmPauseDurationMs = 2500;

// Size of each non-blocking PCM write to the speaker.
static constexpr size_t kSamplePlaybackChunkSize = 512;

// Default RTTTL alarm melody (classic beep pattern).
static constexpr const char *kDefaultAlarmMelody =
    "Alarm:d=8,o=6,b=400:c,p,c,p,c,4p,c,p,c,p,c";

// ---------------------------------------------------------------------------
// Alarm sound definitions — a menu of built-in RTTTL melodies and samples.
// ---------------------------------------------------------------------------

enum class SoundKind : uint8_t {
  kRtttl,
  kSample,
};

struct AlarmSound {
  const char *name;
  SoundKind kind;
  const char *rtttl;
  const uint8_t *sample_data;
  size_t sample_size;
};

static constexpr uint8_t kAlarmSoundCount = 10;

#ifndef UNIT_TEST
#if __has_include("rthawk_pcm.h")
#include "rthawk_pcm.h"
#else
static constexpr uint8_t kRedTailedHawkPcm[] = {};
#endif
#else
static constexpr uint8_t kRedTailedHawkPcm[] = {};
#endif

static const AlarmSound kAlarmSounds[kAlarmSoundCount] = {
    {"Classic Beep", SoundKind::kRtttl, "Alarm:d=8,o=6,b=400:c,p,c,p,c,4p,c,p,c,p,c", nullptr, 0},
    {"Morning Rise", SoundKind::kRtttl, "Morning:d=4,o=5,b=160:c,e,g,8a,8g,e,c,2p,c,e,g,a", nullptr, 0},
    {"Gentle Chime", SoundKind::kRtttl, "Chime:d=8,o=6,b=200:e,g,a,g,e,4p,e,g,a,g,e", nullptr, 0},
    {"Digital Buzz", SoundKind::kRtttl, "Buzz:d=16,o=7,b=600:c,p,c,p,c,p,c,8p,c,p,c,p,c", nullptr, 0},
    {"Melody Wake", SoundKind::kRtttl, "Wake:d=8,o=5,b=180:g,a,b,d6,4b,a,g,4a,2g", nullptr, 0},
    {"Soft Bells", SoundKind::kRtttl, "Bells:d=8,o=6,b=140:e,4p,g,4p,b,2p,g,4p,e,2p", nullptr, 0},
    {"Sunrise", SoundKind::kRtttl, "Sunrise:d=8,o=5,b=150:c,e,g,c6,b,g,e,4g,2c6", nullptr, 0},
    {"Bright Steps", SoundKind::kRtttl, "Steps:d=8,o=5,b=190:c,d,e,g,a,c6,4a,4g,2c6", nullptr, 0},
    {"Urgent Pulse", SoundKind::kRtttl, "Pulse:d=16,o=6,b=480:c,c,p,c,c,p,g,g,p,g,g,4p", nullptr, 0},
    {"Red-tailed Hawk", SoundKind::kSample, nullptr, kRedTailedHawkPcm, sizeof(kRedTailedHawkPcm)},
};

static_assert(kMaxStoredSoundIndex == kAlarmSoundCount,
              "Stored sound-index limit must match the alarm sound catalog");

// Get the RTTTL string for a sound index (clamped to valid range).
inline const char *get_alarm_sound_rtttl(uint8_t index) {
  if (index >= kAlarmSoundCount) {
    index = 0;
  }
  return kAlarmSounds[index].rtttl;
}

inline SoundKind get_alarm_sound_kind(uint8_t index) {
  if (index >= kAlarmSoundCount) {
    index = 0;
  }
  return kAlarmSounds[index].kind;
}

using SampleChunkReader = size_t (*)(void *context, uint32_t offset,
                                     uint8_t *buffer, size_t buffer_size);

// Pure state machine for non-blocking PCM playback. The reader can later be
// backed by an SD file without changing the speaker integration.
class SamplePlaybackState {
 public:
  void start(size_t total_size, bool loop, uint32_t now_ms) {
    total_size_ = total_size;
    loop_ = loop;
    offset_ = 0;
    pause_start_ms_ = now_ms;
    active_ = total_size != 0;
    paused_ = false;
  }

  void stop() {
    active_ = false;
    paused_ = false;
    offset_ = 0;
  }

  bool active() const { return active_; }
  bool paused() const { return paused_; }
  size_t offset() const { return offset_; }

  bool ready(uint32_t now_ms) {
    if (!active_) {
      return false;
    }
    if (paused_) {
      if (now_ms - pause_start_ms_ < kAlarmPauseDurationMs) {
        return false;
      }
      paused_ = false;
      offset_ = 0;
    }
    return true;
  }

  size_t read_next(SampleChunkReader reader, void *context, uint8_t *buffer,
                   size_t buffer_size) const {
    if (!active_ || paused_ || reader == nullptr || buffer == nullptr ||
        offset_ >= total_size_) {
      return 0;
    }
    size_t requested = total_size_ - offset_;
    if (requested > buffer_size) {
      requested = buffer_size;
    }
    return reader(context, static_cast<uint32_t>(offset_), buffer, requested);
  }

  void accept(size_t bytes, uint32_t now_ms) {
    if (!active_) {
      return;
    }
    size_t remaining = total_size_ - offset_;
    if (bytes > remaining) {
      bytes = remaining;
    }
    offset_ += bytes;
    if (offset_ < total_size_) {
      return;
    }
    if (loop_) {
      paused_ = true;
      pause_start_ms_ = now_ms;
    } else {
      active_ = false;
    }
  }

 private:
  size_t total_size_ = 0;
  size_t offset_ = 0;
  uint32_t pause_start_ms_ = 0;
  bool loop_ = false;
  bool active_ = false;
  bool paused_ = false;
};

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

// Only active alarm playback should prevent the display from dimming.
inline bool alarm_should_keep_screen_awake(AlarmState state) {
  return state == AlarmState::kFiring;
}

// ---------------------------------------------------------------------------
// I2C register / value constants for the backlight controller.
// ---------------------------------------------------------------------------
static constexpr uint8_t kBacklightMax = 0;    // PWM duty for maximum brightness.
static constexpr uint8_t kBacklightMin = 244;  // PWM duty for minimum brightness.
static constexpr uint8_t kBacklightOff = 245;  // Value that turns backlight off.
static constexpr uint8_t kBuzzerOn = 246;      // Command to activate buzzer.
static constexpr uint8_t kBuzzerOff = 247;     // Command to deactivate buzzer.
static constexpr uint8_t kStc8BacklightPowerRegister = 0x1B;
static constexpr uint8_t kStc8BacklightPwmRegister = 0x20;
static constexpr uint8_t kStc8AudioShutdownRegister = 0x1A;
// The STC8 switches the panel off below this duty, so it is also the floor for
// any request; dimmer than this is done by shading the content.
static constexpr uint8_t kStc8MinDuty = 5;

enum class BacklightMode : uint8_t {
  kLegacyRaw = 0,
  kStc8Register = 1,
};

enum class SpeakerAmpMode : uint8_t {
  kLegacyCommand = 0,
  kStc8Register = 1,
};

inline uint8_t brightness_to_stc8_duty(float brightness) {
  if (brightness >= 1.0f) {
    return 100;
  }
  if (!(brightness > 0.0f)) {
    return kStc8MinDuty;
  }
  const uint8_t duty = static_cast<uint8_t>(brightness * 100.0f + 0.5f);
  return duty < kStc8MinDuty ? kStc8MinDuty : duty;
}

// ---------------------------------------------------------------------------
// Pure functions — testable on the host without ESPHome.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Screen sleep / wake constants.
// ---------------------------------------------------------------------------

// Idle timeout before dimming the screen (milliseconds).
static constexpr uint32_t kScreenIdleTimeoutMs = 30000;

// While idle the screen drops to this fraction of the awake brightness; it
// must be low enough that a tap is visibly brighter even at the minimum.
static constexpr float kSleepMaxAwakeFraction = 0.5f;

// Adaptive brightness slews at this rate (permille per second), updated every
// kAmbientRampTickMs.
static constexpr float kAmbientRatePermillePerS = 100.0f;
static constexpr uint32_t kAmbientRampTickMs = 50;
// Below the threshold, progressively shade UI colors toward black. The
// backlight stops dimming at kContentDimFloor (kStc8MinDuty), so below it the
// shading alone does the work. Fractions of the original color value:
// 1.0 at the threshold, kContentValueAtFloor at the floor (white is ~0x38) and
// kContentValueAtZero at 0% (white is ~0x08, about the lowest neutral grey the
// panel's RGB565 channels can show; below it channels round to 0 and tint).
static constexpr float kContentDimThreshold = 0.25f;
static constexpr float kContentDimFloor = kStc8MinDuty / 100.0f;
static constexpr float kContentValueAtFloor = 0.22f;
static constexpr float kContentValueAtZero = 0.03f;

inline uint8_t compute_content_dim_opacity(float configured_brightness) {
  float value;
  if (!(configured_brightness > 0.0f)) {
    value = kContentValueAtZero;
  } else if (configured_brightness >= kContentDimThreshold) {
    return 0;
  } else if (configured_brightness >= kContentDimFloor) {
    const float t = (configured_brightness - kContentDimFloor) /
                    (kContentDimThreshold - kContentDimFloor);
    value = kContentValueAtFloor + (1.0f - kContentValueAtFloor) * t;
  } else {
    const float t = configured_brightness / kContentDimFloor;
    value = kContentValueAtZero + (kContentValueAtFloor - kContentValueAtZero) * t;
  }
  return static_cast<uint8_t>((1.0f - value) * 255.0f + 0.5f);
}

// Colors can't get darker than the shading above, so at very low brightness the
// large time switches to an outline font that lights far fewer pixels. The
// exit threshold is higher than the entry one so the font doesn't flicker
// while an ambient reading hovers around the boundary.
static constexpr float kClockOutlineEnter = 0.10f;
static constexpr float kClockOutlineExit = 0.12f;

inline bool compute_clock_outline(float configured_brightness,
                                  bool currently_outline) {
  if (currently_outline) {
    return !(configured_brightness >= kClockOutlineExit);
  }
  return !(configured_brightness > kClockOutlineEnter);
}

// Brightness (0.0-1.0) actually shown: the awake level, or a fixed fraction of
// it while the screen is idle.
inline float compute_screen_brightness(float awake_brightness, bool asleep) {
  if (!(awake_brightness > 0.0f)) {
    return 0.0f;
  }
  if (awake_brightness > 1.0f) {
    awake_brightness = 1.0f;
  }
  return asleep ? awake_brightness * kSleepMaxAwakeFraction : awake_brightness;
}

// Adaptive brightness works in permille (0-1000); settings and the UI use 0.0-1.0.
inline float brightness_to_permille(float brightness) {
  if (!(brightness > 0.0f)) {
    return 0.0f;
  }
  return brightness > 1.0f ? adaptive_brightness::kMaxLevel
                           : brightness * adaptive_brightness::kMaxLevel;
}

inline float permille_to_brightness(float permille) {
  return adaptive_brightness::clamp_level(permille) / adaptive_brightness::kMaxLevel;
}

// Rebuilds a saved curve; false if the stored knots are not a valid curve.
inline bool curve_from_storage(const uint16_t *knots, size_t count,
                               adaptive_brightness::Curve *curve) {
  if (knots == nullptr || curve == nullptr ||
      count != adaptive_brightness::kKnotCount) {
    return false;
  }
  adaptive_brightness::Curve candidate;
  for (size_t i = 0; i < count; ++i) {
    candidate.level[i] = knots[i];
  }
  if (!candidate.plausible()) {
    return false;
  }
  *curve = candidate;
  return true;
}

// Minimum delay between NVS settings writes (milliseconds).
// Prevents flash wear from continuous slider drag events.
static constexpr uint32_t kSettingsDebouncePeriodMs = 2000;

// Wait for sound roller movement to settle before starting a preview.
static constexpr uint32_t kSoundPreviewDebouncePeriodMs = 300;

inline bool deferred_action_ready(uint32_t now_ms, uint32_t started_ms,
                                  uint32_t delay_ms) {
  return now_ms - started_ms >= delay_ms;
}

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

inline uint8_t resolve_backlight_pwm(float brightness, BacklightMode mode) {
  if (mode == BacklightMode::kStc8Register) {
    return brightness_to_stc8_duty(brightness);
  }
  return brightness_to_pwm(brightness);
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
// Output example: "7:00 AM - Work (in 6h 30m)" or "07:00 - Work (in 6h 30m)"
// If |skip_pending| is set, appends a "(skip armed)" suffix so the home page
// shows that the displayed occurrence has a pending skip.
// Returns the number of characters written (excluding null terminator).
inline size_t format_next_alarm_text(const AlarmTime &alarm,
                                     int32_t minutes_until,
                                     bool time_format_24h, char *buf,
                                     size_t buf_size,
                                     bool skip_pending = false) {
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
      written = snprintf(buf, buf_size, "%s - %s (in %uh %um)",
                         time_str, alarm.label, hours, mins);
    } else {
      written = snprintf(buf, buf_size, "%s - %s (in %um)",
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
  size_t total = static_cast<size_t>(written);

  if (skip_pending && total < buf_size) {
    int suffix_written = snprintf(buf + total, buf_size - total, " (skip armed)");
    if (suffix_written > 0) {
      total += static_cast<size_t>(suffix_written);
    }
  }
  return total;
}

// Epoch (UTC seconds) of the next alarm that fires after the current minute,
// honouring skip-next. Returns false if no alarm is scheduled.
inline bool next_alarm_epoch(const AlarmTime *alarms, uint8_t count,
                             int64_t now_epoch, uint8_t now_hour,
                             uint8_t now_minute, uint8_t now_second,
                             uint8_t now_day_index, int64_t *epoch_out) {
  if (alarms == nullptr || epoch_out == nullptr) {
    return false;
  }
  // Search from the following minute so an alarm firing right now is not
  // reported as upcoming.
  const uint16_t following = static_cast<uint16_t>(
      time_to_minutes(now_hour, now_minute) + 1);
  const uint16_t minutes_per_day = 24 * 60;
  const uint8_t day_index = following >= minutes_per_day
                                ? static_cast<uint8_t>((now_day_index + 1) % 7)
                                : now_day_index;
  const uint16_t time_of_day = following % minutes_per_day;
  int32_t minutes = 0;
  if (find_next_alarm_index(alarms, count,
                            static_cast<uint8_t>(time_of_day / 60),
                            static_cast<uint8_t>(time_of_day % 60), day_index,
                            &minutes) < 0) {
    return false;
  }
  *epoch_out = now_epoch - now_second + 60 + static_cast<int64_t>(minutes) * 60;
  return true;
}

// Format UTC epoch seconds as "2026-10-12T14:00:00Z", the form HA parses for
// timestamp entities. Returns the number of characters written, or 0 on error.
inline size_t format_iso8601_utc(int64_t epoch, char *buf, size_t buf_size) {
  if (buf == nullptr || buf_size == 0) {
    return 0;
  }
  const std::time_t t = static_cast<std::time_t>(epoch);
  const std::tm *utc = std::gmtime(&t);
  if (utc == nullptr) {
    buf[0] = '\0';
    return 0;
  }
  const int written = snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                               utc->tm_year + 1900, utc->tm_mon + 1,
                               utc->tm_mday, utc->tm_hour, utc->tm_min,
                               utc->tm_sec);
  if (written < 0) {
    buf[0] = '\0';
    return 0;
  }
  return static_cast<size_t>(written);
}

// Format the pre-alarm banner text.
// Output example: "Alarm in 5 min - Work" or "Alarm in 5 min"
// Returns the number of characters written (excluding null terminator).
inline size_t format_pre_alarm_text(uint16_t minutes_remaining,
                                    const char *label, char *buf,
                                    size_t buf_size) {
  if (buf == nullptr || buf_size == 0) {
    return 0;
  }

  int written;
  if (label != nullptr && label[0] != '\0') {
    written = snprintf(buf, buf_size, "Alarm in %u min - %s",
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
namespace speaker {
class Speaker;
}  // namespace speaker
}  // namespace esphome

namespace alarmclock {

// Maximum number of configurable alarms.
static constexpr uint8_t kMaxAlarms = kMaxAlarmsCount;

class AlarmClockComponent : public ::esphome::Component,
                            public ::esphome::i2c::I2CDevice,
                            public ::esphome::api::CustomAPIDevice {
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
  void update_alarm(uint8_t index, uint8_t hour, uint8_t minute,
                    uint8_t days_mask, const char *label);
  void set_alarm_skip_next(uint8_t index, bool skip);
  void add_alarm();
  void edit_alarm(uint8_t index);
  void delete_alarm(uint8_t index);
  void dismiss_alarm();
  void snooze_alarm();

  // --- Settings ---
  void set_volume(float volume);
  void set_brightness(float brightness);
  void set_min_brightness(float brightness);
  void set_max_brightness(float brightness);
  void set_ambient_full_lux(float lux);
  void reset_learned_brightness();
  // Called with each ambient light sample from the BH1750.
  void on_ambient_lux(float lux);
  void set_sound_index(uint8_t index);
  void preview_sound(uint8_t sound_index);
  void cancel_sound_preview();
  void set_snooze_duration_option(uint8_t option_index);
  void set_time_format_24h(bool time_format_24h);
  void set_pre_alarm_option(uint8_t option_index);
  float volume() const { return volume_; }
  float brightness() const { return brightness_; }
  float min_brightness() const { return min_brightness_; }
  float max_brightness() const { return max_brightness_; }
  float ambient_full_lux() const { return ambient_full_lux_; }
  bool brightness_taught() const { return ambient_.taught(); }
  uint8_t sound_index() const { return selected_sound_index_; }
  bool time_format_24h() const { return time_format_24h_; }
  uint8_t pre_alarm_minutes() const { return pre_alarm_minutes_; }

  // --- RTTTL audio ---
  void set_rtttl(::esphome::rtttl::Rtttl *rtttl) { rtttl_ = rtttl; }
  void set_speaker(::esphome::speaker::Speaker *speaker) { speaker_ = speaker; }
  void set_backlight_mode(uint8_t mode) {
    backlight_mode_ = static_cast<BacklightMode>(mode);
  }
  void set_speaker_amp_mode(uint8_t mode) {
    speaker_amp_mode_ = static_cast<SpeakerAmpMode>(mode);
  }
  void enable_controls_page() { controls_page_enabled_ = true; }
  void enable_minisplit_page() { minisplit_page_enabled_ = true; }
  void add_control(uint8_t type, const std::string &name,
                   const std::string &entity_id);
  ::esphome::Trigger<> *get_control_action_trigger(uint8_t index) {
    return index < theme::kMaxControls ? &control_action_triggers_[index]
                                      : nullptr;
  }
  void set_climate_entity_id(const std::string &entity_id) {
    climate_entity_id_ = entity_id;
  }
  void set_horizontal_vane_entity_id(const std::string &entity_id) {
    horizontal_vane_entity_id_ = entity_id;
  }
  void set_vertical_vane_entity_id(const std::string &entity_id) {
    vertical_vane_entity_id_ = entity_id;
  }
  void set_temperature_limits(float min_temperature, float max_temperature,
                              float temperature_step) {
    min_temperature_ = min_temperature;
    max_temperature_ = max_temperature;
    temperature_step_ = temperature_step;
  }
  void set_temperature_unit(const std::string &unit) {
    temperature_unit_ = unit;
  }
  void add_hvac_mode(const std::string &mode);
  void add_fan_mode(const std::string &mode);
  void on_rtttl_finished();

  // --- Screen sleep/wake ---
  bool is_screen_asleep() const { return screen_asleep_; }

  // --- State queries (for HA sensors) ---
  AlarmState alarm_state() const { return state_machine_.state(); }
  bool is_firing() const {
    return state_machine_.state() == AlarmState::kFiring;
  }

  // ISO 8601 UTC time of the next alarm that will fire, or "" if none.
  // |day_of_week| is 0-based (0 = Sunday).
  std::string next_alarm_iso8601(int64_t now_epoch, uint8_t hour,
                                 uint8_t minute, uint8_t second,
                                 uint8_t day_of_week) const;

  // Called from YAML interval to check alarms against current time.
  void check_alarms_(uint8_t hour, uint8_t minute, uint8_t day_of_week);

 private:
  struct ControlState {
    UiControlType type = UiControlType::kToggle;
    std::string name;
    std::string entity_id;
    std::string state;
    float brightness = NAN;
    bool available = false;
  };

  // Alarm storage.
  AlarmTime alarms_[kMaxAlarms] = {};
  AlarmStateMachine state_machine_;

  // Settings.
  float volume_ = 0.5f;
  // Awake brightness (0.0-1.0): the adaptive level, or the manual level when
  // no ambient light reading is available.
  float brightness_ = 0.5f;
  float min_brightness_ = kDefaultMinBrightness;
  float max_brightness_ = kDefaultMaxBrightness;
  float ambient_full_lux_ = kDefaultAmbientFullLux;
  adaptive_brightness::Controller ambient_;
  uint32_t last_ambient_tick_ms_ = 0;
  uint32_t ambient_sample_count_ = 0;
  uint8_t selected_sound_index_ = 0;
  bool time_format_24h_ = false;
  uint8_t pre_alarm_minutes_ = kDefaultPreAlarmMinutes;

  // RTTTL audio.
  ::esphome::rtttl::Rtttl *rtttl_ = nullptr;
  ::esphome::speaker::Speaker *speaker_ = nullptr;
  SamplePlaybackState sample_playback_;
  uint8_t sample_sound_index_ = 0;
  uint8_t sample_chunk_buffer_[kSamplePlaybackChunkSize] = {};
  bool alarm_sound_active_ = false;
  bool alarm_pause_active_ = false;
  uint32_t alarm_sound_start_ms_ = 0;
  uint32_t alarm_pause_start_ms_ = 0;
  bool sound_preview_pending_ = false;
  uint8_t pending_sound_preview_index_ = 0;
  uint32_t sound_preview_pending_ms_ = 0;

  // Timing.
  uint8_t last_checked_minute_ = 0xFF;
  uint8_t last_known_hour_ = 0;
  uint8_t last_known_minute_ = 0;
  uint8_t last_known_day_of_week_ = 0;

  // Screen sleep state.
  bool screen_asleep_ = false;

  // NVS write debouncing.
  bool settings_dirty_ = false;
  uint32_t settings_dirty_ms_ = 0;

  // Last backlight PWM written to the I2C controller.
  uint8_t last_backlight_pwm_ = 0xFF;
  BacklightMode backlight_mode_ = BacklightMode::kLegacyRaw;
  SpeakerAmpMode speaker_amp_mode_ = SpeakerAmpMode::kLegacyCommand;

  // Tracks whether the home page alarm summary needs refresh.
  bool next_alarm_dirty_ = true;

  bool controls_page_enabled_ = false;
  bool minisplit_page_enabled_ = false;
  std::array<ControlState, theme::kMaxControls> controls_;
  std::array<::esphome::Trigger<>, theme::kMaxControls>
      control_action_triggers_;
  uint8_t control_count_ = 0;

  std::string climate_entity_id_;
  std::string horizontal_vane_entity_id_;
  std::string vertical_vane_entity_id_;
  std::array<std::string, theme::kMaxClimateOptions> hvac_modes_;
  std::array<std::string, theme::kMaxClimateOptions> fan_modes_;
  uint8_t hvac_mode_count_ = 0;
  uint8_t fan_mode_count_ = 0;
  float min_temperature_ = 60.0f;
  float max_temperature_ = 88.0f;
  float temperature_step_ = 1.0f;
  std::string temperature_unit_ = "°F";
  std::string climate_mode_;
  std::string climate_action_;
  std::string climate_fan_mode_;
  std::string horizontal_vane_state_;
  std::string vertical_vane_state_;
  float climate_current_temperature_ = NAN;
  float climate_target_temperature_ = NAN;
  bool climate_available_ = false;

  // Internal helpers.
  void setup_home_assistant_subscriptions_();
  void on_control_state_(uint8_t index, ::esphome::StringRef state);
  void on_control_brightness_(uint8_t index, ::esphome::StringRef state);
  void on_climate_string_state_(std::string *target,
                                ::esphome::StringRef state);
  void on_climate_number_state_(float *target, ::esphome::StringRef state);
  void refresh_minisplit_ui_();

 public:
  // UI event entry points.
  void press_control_(uint8_t index);
  void set_control_brightness_(uint8_t index, float brightness);
  void toggle_minisplit_power_();
  void change_minisplit_temperature_(float delta);
  void set_minisplit_hvac_mode_(const char *mode);
  void set_minisplit_fan_mode_(const char *mode);
  void step_minisplit_vane_(bool horizontal, bool next);

 private:
  void call_ha_action_(const std::string &action,
                       const std::map<std::string, std::string> &data);
  bool is_alarm_configured_(const AlarmTime &alarm) const;
  uint8_t configured_alarm_count_() const;
  void sync_alarm_slots_ui_();
  void mark_next_alarm_dirty_();
  bool consume_alarm_skip_(uint8_t index);
  bool write_backlight_(float brightness);
  bool write_backlight_pwm_(uint8_t pwm);
  void update_backlight_();
  void apply_ambient_limits_();
  void tick_ambient_brightness_();
  bool ambient_shadowed_() const;
  void check_screen_sleep_();
  void start_alarm_sound_();
  void stop_alarm_sound_();
  void finish_active_alarm_();
  void play_alarm_melody_();
  void play_sample_chunk_();
  static size_t read_embedded_sample_(void *context, uint32_t offset,
                                      uint8_t *buffer, size_t buffer_size);
  void play_sound_preview_(uint8_t sound_index);
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
