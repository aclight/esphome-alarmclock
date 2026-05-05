// Alarm clock component implementation.
// This file is only compiled as part of the ESPHome build (not host-side tests).

#include "alarmclock.h"

#ifndef UNIT_TEST

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/rtttl/rtttl.h"

namespace alarmclock {

static const char *const TAG = "alarmclock";

// Pointer to the singleton component (used by UI callbacks).
static AlarmClockComponent *instance_ = nullptr;

// ---------------------------------------------------------------------------
// UI callback handlers (bridge UI events → component methods).
// ---------------------------------------------------------------------------
static void on_dismiss() {
  if (instance_) {
    instance_->dismiss_alarm();
  }
}

static void on_snooze() {
  if (instance_) {
    instance_->snooze_alarm();
  }
}

static void on_volume_change(float volume) {
  if (instance_) {
    instance_->set_volume(volume);
  }
}

static void on_brightness_change(float brightness) {
  if (instance_) {
    instance_->set_brightness(brightness);
  }
}

static void on_alarm_toggle(uint8_t index, bool enabled) {
  if (instance_) {
    instance_->enable_alarm(index, enabled);
  }
}

static void on_alarm_time_set(uint8_t index, uint8_t hour, uint8_t minute) {
  if (instance_) {
    // Keep existing days_mask, just update time.
    instance_->set_alarm(index, hour, minute, 0xFF, true);  // TODO: pass actual days
  }
}

static void on_alarm_days_set(uint8_t index, uint8_t days_mask) {
  (void)index;
  (void)days_mask;
  // TODO: Implement day mask update.
}

// ---------------------------------------------------------------------------
// Component lifecycle.
// ---------------------------------------------------------------------------

void AlarmClockComponent::setup() {
  ESP_LOGI(TAG, "AlarmClock component initializing...");
  instance_ = this;

  // Turn on backlight at maximum brightness.
  uint8_t brightness = kBacklightMax;
  if (this->write(&brightness, 1) != esphome::i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set backlight via I2C (address 0x%02X)",
             this->address_);
  } else {
    ESP_LOGI(TAG, "Backlight set to maximum brightness");
  }

  // Register UI callbacks.
  UiCallbacks cb = {};
  cb.on_alarm_dismiss = on_dismiss;
  cb.on_alarm_snooze = on_snooze;
  cb.on_volume_change = on_volume_change;
  cb.on_brightness_change = on_brightness_change;
  cb.on_alarm_toggle = on_alarm_toggle;
  cb.on_alarm_time_set = on_alarm_time_set;
  cb.on_alarm_days_set = on_alarm_days_set;
  ui_set_callbacks(cb);

  // Build the UI (LVGL must already be initialized by the lvgl: component).
  ui_init();

  // Set a default alarm for demo purposes.
  // TODO: Load from NVS/flash persistent storage.
  set_alarm(0, 7, 0, alarm_clock::kWeekdays, true);

  ESP_LOGI(TAG, "AlarmClock ready");
}

void AlarmClockComponent::loop() {
  // Handle alarm sound pause between RTTTL loops.
  if (alarm_sound_active_ && alarm_pause_active_) {
    uint32_t now_ms = millis();
    if (now_ms - alarm_pause_start_ms_ >= kAlarmPauseDurationMs) {
      alarm_pause_active_ = false;
      play_alarm_melody_();
    }
  }

  // Tick the snooze timer once per minute.
  uint32_t now_ms = millis();
  if (now_ms - last_minute_check_ms_ >= 60000) {
    last_minute_check_ms_ = now_ms;
    if (state_machine_.tick_snooze()) {
      // Snooze expired — alarm fires again.
      start_alarm_sound_();
      ui_show_firing_overlay();
    }
  }
}

// ---------------------------------------------------------------------------
// Alarm management.
// ---------------------------------------------------------------------------

void AlarmClockComponent::set_alarm(uint8_t index, uint8_t hour, uint8_t minute,
                                    uint8_t days_mask, bool enabled) {
  set_alarm(index, hour, minute, days_mask, enabled, nullptr);
}

void AlarmClockComponent::set_alarm(uint8_t index, uint8_t hour, uint8_t minute,
                                    uint8_t days_mask, bool enabled,
                                    const char *label) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarms_[index].hour = hour;
  alarms_[index].minute = minute;
  alarms_[index].days_of_week = days_mask;
  alarms_[index].enabled = enabled;
  alarm_clock::alarm_set_label(alarms_[index], label);

  // Update the UI alarm list.
  ui_update_alarm_row(index, hour, minute, days_mask, enabled,
                      alarms_[index].label);

  ESP_LOGI(TAG, "Alarm %d set: %02d:%02d days=0x%02X enabled=%d label='%s'",
           index, hour, minute, days_mask, enabled, alarms_[index].label);
}

void AlarmClockComponent::enable_alarm(uint8_t index, bool enabled) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarms_[index].enabled = enabled;
  ui_update_alarm_row(index, alarms_[index].hour, alarms_[index].minute,
                      alarms_[index].days_of_week, enabled,
                      alarms_[index].label);
}

void AlarmClockComponent::dismiss_alarm() {
  ESP_LOGI(TAG, "Alarm dismissed");
  state_machine_.dismiss();
  stop_alarm_sound_();
  ui_hide_firing_overlay();
  ui_firing_stop_animation();
}

void AlarmClockComponent::snooze_alarm() {
  if (state_machine_.snooze()) {
    ESP_LOGI(TAG, "Alarm snoozed (%d min)",
             state_machine_.snooze_duration_minutes());
    stop_alarm_sound_();
    ui_hide_firing_overlay();
    ui_firing_stop_animation();
  } else {
    // Max snoozes reached — auto-dismissed.
    ESP_LOGI(TAG, "Max snoozes reached, dismissed");
    ui_hide_firing_overlay();
    ui_firing_stop_animation();
  }
}

// ---------------------------------------------------------------------------
// Settings.
// ---------------------------------------------------------------------------

void AlarmClockComponent::set_volume(float volume) {
  if (volume < 0.0f) { volume = 0.0f; }
  if (volume > 1.0f) { volume = 1.0f; }
  volume_ = volume;
  ui_update_volume(volume);
  // TODO: Apply to speaker/RTTTL gain.
}

void AlarmClockComponent::set_brightness(float brightness) {
  if (brightness < 0.0f) { brightness = 0.0f; }
  if (brightness > 1.0f) { brightness = 1.0f; }
  brightness_ = brightness;
  update_backlight_();
  ui_update_brightness(brightness);
}

void AlarmClockComponent::set_sensor_factor(float sensor_factor) {
  if (sensor_factor < 0.0f) { sensor_factor = 0.0f; }
  if (sensor_factor > 1.0f) { sensor_factor = 1.0f; }
  sensor_factor_ = sensor_factor;
  update_backlight_();
}

// ---------------------------------------------------------------------------
// Internal helpers.
// ---------------------------------------------------------------------------

void AlarmClockComponent::check_alarms_(uint8_t hour, uint8_t minute,
                                        uint8_t day_of_week) {
  if (state_machine_.state() != alarm_clock::AlarmState::kIdle) {
    return;  // Already firing or snoozed.
  }
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    if (alarm_clock::alarm_matches(alarms_[i], hour, minute, day_of_week)) {
      ESP_LOGW(TAG, "Alarm %d triggered!", i);
      state_machine_.trigger();
      start_alarm_sound_();
      ui_show_firing_overlay();
      ui_firing_start_animation();
      ui_firing_update_time(hour, minute);
      ui_firing_update_label(alarms_[i].label);
      break;
    }
  }
}

void AlarmClockComponent::update_backlight_() {
  float bright = compute_brightness(brightness_, sensor_factor_);
  uint8_t pwm = brightness_to_pwm(bright);
  this->write(&pwm, 1);
}

void AlarmClockComponent::start_alarm_sound_() {
  ESP_LOGI(TAG, "Starting alarm sound (volume=%.0f%%)", volume_ * 100);
  alarm_sound_active_ = true;
  alarm_pause_active_ = false;
  alarm_sound_start_ms_ = millis();
  play_alarm_melody_();
}

void AlarmClockComponent::stop_alarm_sound_() {
  ESP_LOGI(TAG, "Stopping alarm sound");
  alarm_sound_active_ = false;
  alarm_pause_active_ = false;
  if (rtttl_ != nullptr) {
    rtttl_->stop();
  }
}

void AlarmClockComponent::play_alarm_melody_() {
  if (rtttl_ == nullptr) {
    return;
  }
  uint32_t elapsed = millis() - alarm_sound_start_ms_;
  float gain = compute_ramp_volume(volume_, elapsed);
  rtttl_->set_gain(gain);
  rtttl_->play(kDefaultAlarmMelody);
  ESP_LOGD(TAG, "Playing alarm melody (gain=%.2f, elapsed=%ums)", gain,
           elapsed);
}

void AlarmClockComponent::on_rtttl_finished() {
  if (!alarm_sound_active_) {
    return;
  }
  // Start a pause before the next melody loop.
  alarm_pause_active_ = true;
  alarm_pause_start_ms_ = millis();
  ESP_LOGD(TAG, "RTTTL finished, pausing %ums before next loop",
           kAlarmPauseDurationMs);
}

void AlarmClockComponent::sync_ui_() {
  // Sync UI state with component state (e.g., after loading from NVS).
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    if (alarms_[i].days_of_week != 0) {
      ui_update_alarm_row(i, alarms_[i].hour, alarms_[i].minute,
                          alarms_[i].days_of_week, alarms_[i].enabled,
                          alarms_[i].label);
    }
  }
  ui_update_volume(volume_);
  ui_update_brightness(brightness_);
}

}  // namespace alarmclock

#endif  // UNIT_TEST
