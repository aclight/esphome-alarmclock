// NVS persistence implementation using ESP-IDF's nvs_flash API.
// This file is only compiled as part of the ESPHome build (not host-side tests).

#include "storage.h"

#ifndef UNIT_TEST

#include <nvs_flash.h>
#include <nvs.h>
#include "esphome/core/log.h"

namespace alarm_clock {

static const char *const TAG = "alarm_storage";

// NVS namespace for all alarm clock data.
static constexpr const char *kNvsNamespace = "alarmclock";

// NVS key for global settings.
static constexpr const char *kSettingsKey = "settings";

// Buffer for NVS key names (e.g. "alarm_0" .. "alarm_3").
static constexpr size_t kKeyBufSize = 12;

static void make_alarm_key(uint8_t index, char *buf, size_t buf_size) {
  snprintf(buf, buf_size, "alarm_%u", index);
}

bool storage_init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS partition needs erase (err=0x%X), erasing...", err);
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS init failed: 0x%X", err);
    return false;
  }
  ESP_LOGI(TAG, "NVS initialized");
  return true;
}

void storage_save_alarm(uint8_t index, const AlarmTime &alarm) {
  uint8_t buf[kSerializedAlarmSize];
  size_t written = serialize_alarm(alarm, buf, sizeof(buf));
  if (written == 0) {
    ESP_LOGE(TAG, "Failed to serialize alarm %u", index);
    return;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS open failed: 0x%X", err);
    return;
  }

  char key[kKeyBufSize];
  make_alarm_key(index, key, sizeof(key));

  err = nvs_set_blob(handle, key, buf, written);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS set_blob '%s' failed: 0x%X", key, err);
  } else {
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "NVS commit failed: 0x%X", err);
    } else {
      ESP_LOGD(TAG, "Saved alarm %u to NVS", index);
    }
  }

  nvs_close(handle);
}

bool storage_load_alarm(uint8_t index, AlarmTime *alarm) {
  if (alarm == nullptr) {
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "NVS open failed (no data yet?): 0x%X", err);
    return false;
  }

  char key[kKeyBufSize];
  make_alarm_key(index, key, sizeof(key));

  uint8_t buf[kSerializedAlarmSize];
  size_t length = sizeof(buf);
  err = nvs_get_blob(handle, key, buf, &length);
  nvs_close(handle);

  if (err != ESP_OK) {
    ESP_LOGD(TAG, "NVS get_blob '%s' failed: 0x%X", key, err);
    return false;
  }

  if (!deserialize_alarm(buf, length, alarm)) {
    ESP_LOGW(TAG, "Failed to deserialize alarm %u (corrupt or version mismatch)", index);
    return false;
  }

  ESP_LOGD(TAG, "Loaded alarm %u: %02u:%02u days=0x%02X enabled=%d label='%s'",
           index, alarm->hour, alarm->minute, alarm->days_of_week,
           alarm->enabled, alarm->label);
  return true;
}

void storage_save_settings(const StorageSettings &settings) {
  uint8_t buf[kSerializedSettingsSize];
  size_t written = serialize_settings(settings, buf, sizeof(buf));
  if (written == 0) {
    ESP_LOGE(TAG, "Failed to serialize settings");
    return;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS open failed: 0x%X", err);
    return;
  }

  err = nvs_set_blob(handle, kSettingsKey, buf, written);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS set_blob '%s' failed: 0x%X", kSettingsKey, err);
  } else {
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "NVS commit failed: 0x%X", err);
    } else {
      ESP_LOGD(TAG, "Saved settings to NVS");
    }
  }

  nvs_close(handle);
}

bool storage_load_settings(StorageSettings *settings) {
  if (settings == nullptr) {
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "NVS open failed (no data yet?): 0x%X", err);
    return false;
  }

  uint8_t buf[kSerializedSettingsSize];
  size_t length = sizeof(buf);
  err = nvs_get_blob(handle, kSettingsKey, buf, &length);
  nvs_close(handle);

  if (err != ESP_OK) {
    ESP_LOGD(TAG, "NVS get_blob '%s' failed: 0x%X", kSettingsKey, err);
    return false;
  }

  if (!deserialize_settings(buf, length, settings)) {
    ESP_LOGW(TAG, "Failed to deserialize settings (corrupt or version mismatch)");
    return false;
  }

  ESP_LOGD(TAG, "Loaded settings: vol=%.2f bri=%.2f snooze=%u 24h=%d sound=%u",
           settings->volume, settings->brightness,
           settings->snooze_duration_minutes, settings->time_format_24h,
           settings->selected_sound_index);
  return true;
}

}  // namespace alarm_clock

#endif  // UNIT_TEST
