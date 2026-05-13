// NVS persistence for alarm clock settings and alarm configurations.
// Pure serialization/deserialization logic is decoupled from ESP-IDF so it
// can be tested on the host with g++ under -DUNIT_TEST.
#ifndef STORAGE_H
#define STORAGE_H

#include <cstdint>
#include <cstring>

#include "alarm_time.h"

namespace alarm_clock {

// ---------------------------------------------------------------------------
// Serialization format constants.
// ---------------------------------------------------------------------------

// Current format version — bump when the layout changes.
static constexpr uint8_t kStorageVersion = 2;

// Serialized sizes (including the version byte).
static constexpr size_t kSerializedAlarmSize = 1 + 1 + 1 + 1 + 1 + kAlarmLabelMaxLen;  // 21
static constexpr size_t kSerializedSettingsSize = 1 + 4 + 4 + 1 + 1 + 1 + 1;               // 13

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
  return offset;
}

// Deserialize an AlarmTime from |buf| (must be >= kSerializedAlarmSize bytes).
// Returns true on success.
inline bool deserialize_alarm(const uint8_t *buf, size_t buf_size, AlarmTime *alarm) {
  if (buf == nullptr || alarm == nullptr || buf_size < kSerializedAlarmSize) {
    return false;
  }
  size_t offset = 0;
  uint8_t version = buf[offset++];
  if (version != kStorageVersion) {
    return false;
  }
  alarm->hour = buf[offset++];
  alarm->minute = buf[offset++];
  alarm->days_of_week = buf[offset++];
  alarm->enabled = buf[offset++] != 0;
  std::memcpy(alarm->label, &buf[offset], kAlarmLabelMaxLen);
  alarm->label[kAlarmLabelMaxLen - 1] = '\0';  // Ensure null termination.
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
  return offset;
}

// Deserialize StorageSettings from |buf| (must be >= kSerializedSettingsSize bytes).
// Returns true on success.
inline bool deserialize_settings(const uint8_t *buf, size_t buf_size,
                                 StorageSettings *settings) {
  if (buf == nullptr || settings == nullptr || buf_size < kSerializedSettingsSize) {
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

}  // namespace alarm_clock

#endif  // STORAGE_H
