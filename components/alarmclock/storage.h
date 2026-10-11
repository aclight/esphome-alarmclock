// NVS persistence for alarm clock settings and alarm configurations.
// Pure serialization/deserialization logic is decoupled from ESP-IDF so it
// can be tested on the host with g++ under -DUNIT_TEST.
#ifndef STORAGE_H
#define STORAGE_H

#include <cmath>
#include <cstdint>
#include <cstring>

#include "alarm_time.h"

namespace alarmclock {

// ---------------------------------------------------------------------------
// Serialization format constants.
// ---------------------------------------------------------------------------

// Base format version. Append-only settings fields remain compatible by size.
static constexpr uint8_t kStorageVersion = 2;

// Maximum valid alarm sound index for storage validation.
// Must be kept in sync with kAlarmSoundCount in alarmclock.h.
static constexpr uint8_t kMaxStoredSoundIndex = 10;

// Serialized sizes (including the version byte).
static constexpr size_t kLegacySerializedAlarmSize = 1 + 1 + 1 + 1 + 1 + kAlarmLabelMaxLen;  // 21
static constexpr size_t kSerializedAlarmSize = kLegacySerializedAlarmSize + 1;  // 22
static constexpr size_t kLegacySerializedSettingsSize = 1 + 4 + 4 + 1 + 1 + 1 + 1;  // 13
// Adds min_brightness (previously the night brightness fraction).
static constexpr size_t kMinBrightnessSettingsSize =
    kLegacySerializedSettingsSize + 4;  // 17

// Knots in the learned ambient brightness curve; must equal
// adaptive_brightness::kKnotCount (checked by a static_assert in alarmclock.h).
static constexpr size_t kStorageCurveKnots = 9;

// Adds max_brightness, ambient_full_lux, curve_taught and the curve knots.
static constexpr size_t kSerializedSettingsSize =
    kMinBrightnessSettingsSize + 4 + 4 + 1 + 2 * kStorageCurveKnots;  // 44

static constexpr float kDefaultMinBrightness = 0.05f;
static constexpr float kDefaultMaxBrightness = 1.0f;
// Ambient lux at which the untaught curve reaches max brightness.
static constexpr float kDefaultAmbientFullLux = 1.0f;
static constexpr float kMaxAmbientFullLux = 10000.0f;

// ---------------------------------------------------------------------------
// Global settings stored in NVS.
// ---------------------------------------------------------------------------

struct StorageSettings {
  float volume = 0.5f;
  float brightness = 0.5f;
  uint8_t snooze_duration_minutes = 9;
  bool time_format_24h = false;
  uint8_t selected_sound_index = 0;
  uint8_t pre_alarm_minutes = 5;
  float min_brightness = kDefaultMinBrightness;
  float max_brightness = kDefaultMaxBrightness;
  float ambient_full_lux = kDefaultAmbientFullLux;
  bool curve_taught = false;
  uint16_t curve[kStorageCurveKnots] = {};
};

// ---------------------------------------------------------------------------
// Pure serialization / deserialization — testable on host.
// ---------------------------------------------------------------------------

// Serialize an AlarmTime into |buf| (must be >= kSerializedAlarmSize bytes).
// Returns the number of bytes written.
inline size_t serialize_alarm(const AlarmTime &alarm, uint8_t *buf, size_t buf_size) {
  if (buf == nullptr || buf_size < kSerializedAlarmSize) {
    return 0;
  }
  size_t offset = 0;
  buf[offset++] = kStorageVersion;
  buf[offset++] = alarm.hour;
  buf[offset++] = alarm.minute;
  buf[offset++] = alarm.days_of_week;
  buf[offset++] = alarm.enabled ? 1 : 0;
  std::memcpy(&buf[offset], alarm.label, kAlarmLabelMaxLen);
  offset += kAlarmLabelMaxLen;
  buf[offset++] = alarm.skip_next ? 1 : 0;
  return offset;
}

// Deserialize current alarm or a legacy record without skip_next.
// Returns true on success.  Returns false if any field is out of range.
inline bool deserialize_alarm(const uint8_t *buf, size_t buf_size, AlarmTime *alarm) {
  if (buf == nullptr || alarm == nullptr || buf_size < kLegacySerializedAlarmSize) {
    return false;
  }
  size_t offset = 0;
  uint8_t version = buf[offset++];
  if (version != kStorageVersion) {
    return false;
  }
  uint8_t hour = buf[offset++];
  uint8_t minute = buf[offset++];
  uint8_t days = buf[offset++];
  bool enabled = buf[offset++] != 0;

  // Validate fields — reject corrupted data.
  if (hour > 23 || minute > 59 || (days & 0x80) != 0) {
    return false;
  }

  alarm->hour = hour;
  alarm->minute = minute;
  alarm->days_of_week = days;
  alarm->enabled = enabled;
  std::memcpy(alarm->label, &buf[offset], kAlarmLabelMaxLen);
  alarm->label[kAlarmLabelMaxLen - 1] = '\0';  // Ensure null termination.
  offset += kAlarmLabelMaxLen;

  alarm->skip_next = (buf_size >= kSerializedAlarmSize) ? (buf[offset] != 0) : false;
  return true;
}

// Serialize StorageSettings into |buf| (must be >= kSerializedSettingsSize bytes).
// Returns the number of bytes written.
inline size_t serialize_settings(const StorageSettings &settings, uint8_t *buf,
                                 size_t buf_size) {
  if (buf == nullptr || buf_size < kSerializedSettingsSize) {
    return 0;
  }
  size_t offset = 0;
  buf[offset++] = kStorageVersion;
  std::memcpy(&buf[offset], &settings.volume, sizeof(float));
  offset += sizeof(float);
  std::memcpy(&buf[offset], &settings.brightness, sizeof(float));
  offset += sizeof(float);
  buf[offset++] = settings.snooze_duration_minutes;
  buf[offset++] = settings.time_format_24h ? 1 : 0;
  buf[offset++] = settings.selected_sound_index;
  buf[offset++] = settings.pre_alarm_minutes;
  std::memcpy(&buf[offset], &settings.min_brightness, sizeof(float));
  offset += sizeof(float);
  std::memcpy(&buf[offset], &settings.max_brightness, sizeof(float));
  offset += sizeof(float);
  std::memcpy(&buf[offset], &settings.ambient_full_lux, sizeof(float));
  offset += sizeof(float);
  buf[offset++] = settings.curve_taught ? 1 : 0;
  for (size_t i = 0; i < kStorageCurveKnots; ++i) {
    std::memcpy(&buf[offset], &settings.curve[i], sizeof(uint16_t));
    offset += sizeof(uint16_t);
  }
  return offset;
}

// Deserialize current settings or an older record missing the newer fields.
// Returns true on success. Clamps out-of-range values to valid defaults.
inline bool deserialize_settings(const uint8_t *buf, size_t buf_size,
                                 StorageSettings *settings) {
  if (buf == nullptr || settings == nullptr ||
      buf_size < kLegacySerializedSettingsSize) {
    return false;
  }
  size_t offset = 0;
  uint8_t version = buf[offset++];
  if (version != kStorageVersion) {
    return false;
  }
  std::memcpy(&settings->volume, &buf[offset], sizeof(float));
  offset += sizeof(float);
  std::memcpy(&settings->brightness, &buf[offset], sizeof(float));
  offset += sizeof(float);
  settings->snooze_duration_minutes = buf[offset++];
  settings->time_format_24h = buf[offset++] != 0;
  settings->selected_sound_index = buf[offset++];
  settings->pre_alarm_minutes = buf[offset++];
  settings->min_brightness = kDefaultMinBrightness;
  if (buf_size >= kMinBrightnessSettingsSize) {
    std::memcpy(&settings->min_brightness, &buf[offset], sizeof(float));
  }
  offset += sizeof(float);
  settings->max_brightness = kDefaultMaxBrightness;
  settings->ambient_full_lux = kDefaultAmbientFullLux;
  settings->curve_taught = false;
  for (size_t i = 0; i < kStorageCurveKnots; ++i) {
    settings->curve[i] = 0;
  }
  if (buf_size >= kSerializedSettingsSize) {
    std::memcpy(&settings->max_brightness, &buf[offset], sizeof(float));
    offset += sizeof(float);
    std::memcpy(&settings->ambient_full_lux, &buf[offset], sizeof(float));
    offset += sizeof(float);
    settings->curve_taught = buf[offset++] != 0;
    for (size_t i = 0; i < kStorageCurveKnots; ++i) {
      std::memcpy(&settings->curve[i], &buf[offset], sizeof(uint16_t));
      offset += sizeof(uint16_t);
    }
  }

  // Clamp volume and brightness to [0.0, 1.0].
  if (settings->volume < 0.0f || settings->volume > 1.0f ||
      std::isnan(settings->volume) || std::isinf(settings->volume)) {
    settings->volume = 0.5f;
  }
  if (settings->brightness < 0.0f || settings->brightness > 1.0f ||
      std::isnan(settings->brightness) || std::isinf(settings->brightness)) {
    settings->brightness = 0.5f;
  }
  if (!(settings->min_brightness >= 0.0f && settings->min_brightness <= 1.0f)) {
    settings->min_brightness = kDefaultMinBrightness;
  }
  if (!(settings->max_brightness >= 0.0f && settings->max_brightness <= 1.0f)) {
    settings->max_brightness = kDefaultMaxBrightness;
  }
  if (!(settings->ambient_full_lux > 0.0f &&
        settings->ambient_full_lux <= kMaxAmbientFullLux)) {
    settings->ambient_full_lux = kDefaultAmbientFullLux;
  }
  // Clamp sound index to valid range.
  if (settings->selected_sound_index >= kMaxStoredSoundIndex) {
    settings->selected_sound_index = 0;
  }
  return true;
}

// ---------------------------------------------------------------------------
// NVS persistence API (ESP-IDF only, excluded from host-side tests).
// ---------------------------------------------------------------------------
#ifndef UNIT_TEST

// Initialize the NVS storage subsystem.  Call once during setup().
// Returns true on success.
bool storage_init();

// Save a single alarm to NVS.
void storage_save_alarm(uint8_t index, const AlarmTime &alarm);

// Load a single alarm from NVS.  Returns true if a valid entry was found.
bool storage_load_alarm(uint8_t index, AlarmTime *alarm);

// Save global settings to NVS.
void storage_save_settings(const StorageSettings &settings);

// Load global settings from NVS.  Returns true if a valid entry was found.
bool storage_load_settings(StorageSettings *settings);

#endif  // UNIT_TEST

}  // namespace alarmclock

#endif  // STORAGE_H
