// Alarm clock component implementation.
// This file is only compiled as part of the ESPHome build (not host-side tests).

#include "alarmclock.h"

#ifndef UNIT_TEST

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/rtttl/rtttl.h"
#include "esphome/components/api/api_server.h"

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
    instance_->update_alarm_time(index, hour, minute);
  }
}

static void on_alarm_days_set(uint8_t index, uint8_t days_mask) {
  if (instance_) {
    instance_->update_alarm_days(index, days_mask);
  }
}

static void on_sound_change(uint8_t sound_index) {
  if (instance_) {
    instance_->set_sound_index(sound_index);
  }
}

static void on_sound_preview(uint8_t sound_index) {
  if (instance_) {
    instance_->preview_sound(sound_index);
  }
}

static void on_snooze_duration_change(uint8_t option_index) {
  if (instance_) {
    instance_->set_snooze_duration_option(option_index);
  }
}

static void on_time_format_change(bool time_format_24h) {
  if (instance_) {
    instance_->set_time_format_24h(time_format_24h);
  }
}

static void on_pre_alarm_change(uint8_t option_index) {
  if (instance_) {
    instance_->set_pre_alarm_option(option_index);
  }
}

static void on_alarm_edit(uint8_t index) {
  if (instance_) {
    instance_->edit_alarm(index);
  }
}

static void on_alarm_delete(uint8_t index) {
  if (instance_) {
    instance_->delete_alarm(index);
  }
}

static void on_alarm_label_set(uint8_t index, const char *label) {
  if (instance_) {
    instance_->update_alarm_label(index, label);
  }
}

// ---------------------------------------------------------------------------
// Component lifecycle.
// ---------------------------------------------------------------------------

void AlarmClockComponent::setup() {
  ESP_LOGI(TAG, "AlarmClock component initializing...");
  instance_ = this;

  // Turn on backlight at maximum brightness.
  uint8_t brightness = kBacklightMax;
  if (this->write(&brightness, 1) != ::esphome::i2c::ERROR_OK) {
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
  cb.on_alarm_edit = on_alarm_edit;
  cb.on_alarm_delete = on_alarm_delete;
  cb.on_alarm_label_set = on_alarm_label_set;
  cb.on_sound_change = on_sound_change;
  cb.on_sound_preview = on_sound_preview;
  cb.on_snooze_duration_change = on_snooze_duration_change;
  cb.on_time_format_change = on_time_format_change;
  cb.on_pre_alarm_change = on_pre_alarm_change;
  ui_set_callbacks(cb);

  // Build the UI (LVGL must already be initialized by the lvgl: component).
  ui_init();

  // Initialize NVS and load persisted alarms + settings.
  storage_init();
  load_from_storage_();

  ESP_LOGI(TAG, "AlarmClock ready");
}

void AlarmClockComponent::loop() {
  // Flush deferred NVS writes (debounce slider drags).
  if (settings_dirty_) {
    uint32_t now_ms = ::esphome::millis();
    if (now_ms - settings_dirty_ms_ >= kSettingsDebouncePeriodMs) {
      settings_dirty_ = false;
      save_settings_to_storage_();
    }
  }

  // Check screen sleep/wake state based on LVGL inactivity.
  check_screen_sleep_();

  // Handle alarm sound pause between RTTTL loops.
  if (alarm_sound_active_ && alarm_pause_active_) {
    uint32_t now_ms = ::esphome::millis();
    if (now_ms - alarm_pause_start_ms_ >= kAlarmPauseDurationMs) {
      alarm_pause_active_ = false;
      play_alarm_melody_();
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
  alarm_set_label(alarms_[index], label);

  // Persist to NVS.
  storage_save_alarm(index, alarms_[index]);

  // Update the UI alarm list.
  ui_update_alarm_row(index, hour, minute, days_mask, enabled,
                      time_format_24h_, alarms_[index].label);

  // Immediately refresh the next-alarm countdown on the clock page.
  update_next_alarm_display_(last_known_hour_, last_known_minute_,
                             last_known_day_of_week_);

  ESP_LOGI(TAG, "Alarm %d set: %02d:%02d days=0x%02X enabled=%d label='%s'",
           index, hour, minute, days_mask, enabled, alarms_[index].label);
}

void AlarmClockComponent::enable_alarm(uint8_t index, bool enabled) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarms_[index].enabled = enabled;
  storage_save_alarm(index, alarms_[index]);
  ui_update_alarm_row(index, alarms_[index].hour, alarms_[index].minute,
                      alarms_[index].days_of_week, enabled,
                      time_format_24h_, alarms_[index].label);
  update_next_alarm_display_(last_known_hour_, last_known_minute_,
                             last_known_day_of_week_);
}

void AlarmClockComponent::update_alarm_time(uint8_t index, uint8_t hour,
                                            uint8_t minute) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarms_[index].hour = hour;
  alarms_[index].minute = minute;
  storage_save_alarm(index, alarms_[index]);
  ui_update_alarm_row(index, hour, minute, alarms_[index].days_of_week,
                      alarms_[index].enabled, time_format_24h_,
                      alarms_[index].label);
  update_next_alarm_display_(last_known_hour_, last_known_minute_,
                             last_known_day_of_week_);
  ESP_LOGI(TAG, "Alarm %d time updated: %02d:%02d", index, hour, minute);
}

void AlarmClockComponent::update_alarm_days(uint8_t index, uint8_t days_mask) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarms_[index].days_of_week = days_mask;
  storage_save_alarm(index, alarms_[index]);
  ui_update_alarm_row(index, alarms_[index].hour, alarms_[index].minute,
                      days_mask, alarms_[index].enabled, time_format_24h_,
                      alarms_[index].label);
  update_next_alarm_display_(last_known_hour_, last_known_minute_,
                             last_known_day_of_week_);
  ESP_LOGI(TAG, "Alarm %d days updated: 0x%02X", index, days_mask);
}

void AlarmClockComponent::update_alarm_label(uint8_t index, const char *label) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarm_set_label(alarms_[index], label);
  storage_save_alarm(index, alarms_[index]);
  ui_update_alarm_row(index, alarms_[index].hour, alarms_[index].minute,
                      alarms_[index].days_of_week, alarms_[index].enabled,
                      time_format_24h_, alarms_[index].label);
  ESP_LOGI(TAG, "Alarm %d label updated: '%s'", index, alarms_[index].label);
}

void AlarmClockComponent::edit_alarm(uint8_t index) {
  if (index >= kMaxAlarms) {
    return;
  }
  ui_show_time_picker(index, alarms_[index].hour, alarms_[index].minute,
                      alarms_[index].days_of_week, alarms_[index].label,
                      time_format_24h_);
  ESP_LOGI(TAG, "Editing alarm %d", index);
}

void AlarmClockComponent::delete_alarm(uint8_t index) {
  if (index >= kMaxAlarms) {
    return;
  }
  alarms_[index] = AlarmTime{};  // Reset to defaults.
  storage_save_alarm(index, alarms_[index]);
  ui_hide_alarm_row(index);
  update_next_alarm_display_(last_known_hour_, last_known_minute_,
                             last_known_day_of_week_);
  ESP_LOGI(TAG, "Alarm %d deleted", index);
}

void AlarmClockComponent::dismiss_alarm() {
  ESP_LOGI(TAG, "Alarm dismissed");
  state_machine_.dismiss();
  stop_alarm_sound_();
  ui_hide_firing_overlay();
  ui_firing_stop_animation();
  if (fired_alarm_index_ < kMaxAlarms) {
    ui_clear_alarm_row_firing(fired_alarm_index_);
  }
  auto_disable_one_shot_alarm_();
  fired_alarm_index_ = 0xFF;
  fire_ha_event_("esphome.alarm_dismissed");
}

void AlarmClockComponent::snooze_alarm() {
  if (state_machine_.snooze()) {
    ESP_LOGI(TAG, "Alarm snoozed (%d min)",
             state_machine_.snooze_duration_minutes());
    stop_alarm_sound_();
    ui_hide_firing_overlay();
    ui_firing_stop_animation();
    // Show snooze indication on the clock page.
    char buf[48];
    snprintf(buf, sizeof(buf), "Snoozed - resumes in %u min",
             state_machine_.snooze_remaining_minutes());
    ui_update_pre_alarm_banner(buf);
    fire_ha_event_("esphome.alarm_snoozed");
  } else {
    // Max snoozes reached — auto-dismissed.
    ESP_LOGI(TAG, "Max snoozes reached, dismissed");
    ui_hide_firing_overlay();
    ui_firing_stop_animation();
    if (fired_alarm_index_ < kMaxAlarms) {
      ui_clear_alarm_row_firing(fired_alarm_index_);
    }
    auto_disable_one_shot_alarm_();
    fired_alarm_index_ = 0xFF;
    fire_ha_event_("esphome.alarm_dismissed");
  }
}

// ---------------------------------------------------------------------------
// Settings.
// ---------------------------------------------------------------------------

void AlarmClockComponent::set_volume(float volume) {
  if (volume < 0.0f) { volume = 0.0f; }
  if (volume > 1.0f) { volume = 1.0f; }
  volume_ = volume;
  mark_settings_dirty_();
  ui_update_volume(volume);
  ESP_LOGI(TAG, "Volume set to %.0f%%", volume_ * 100.0f);
  // TODO: Apply to speaker/RTTTL gain.
}

void AlarmClockComponent::set_brightness(float brightness) {
  if (brightness < 0.0f) { brightness = 0.0f; }
  if (brightness > 1.0f) { brightness = 1.0f; }
  brightness_ = brightness;
  mark_settings_dirty_();
  update_backlight_();
  ui_update_brightness(brightness);
  ESP_LOGI(TAG, "Brightness set to %.0f%%", brightness_ * 100.0f);
}

void AlarmClockComponent::set_sensor_factor(float sensor_factor) {
  if (sensor_factor < 0.0f) { sensor_factor = 0.0f; }
  if (sensor_factor > 1.0f) { sensor_factor = 1.0f; }

  // Ignore tiny sensor jitter to reduce backlight shimmer.
  if (fabsf(sensor_factor - sensor_factor_) < kSensorFactorDeadband) {
    return;
  }

  // Smooth larger transitions so brightness changes feel stable.
  sensor_factor_ = sensor_factor_ +
      (sensor_factor - sensor_factor_) * kSensorFactorAlpha;
  update_backlight_();
}

void AlarmClockComponent::set_sound_index(uint8_t index) {
  if (index >= kAlarmSoundCount) {
    index = 0;
  }
  selected_sound_index_ = index;
  save_settings_to_storage_();
  ui_update_sound_selection(index);
  ESP_LOGI(TAG, "Alarm sound set to: %s", get_alarm_sound_name(index));
}

void AlarmClockComponent::preview_sound(uint8_t sound_index) {
  if (rtttl_ == nullptr) {
    return;
  }
  if (sound_index >= kAlarmSoundCount) {
    sound_index = 0;
  }
  rtttl_->set_gain(volume_);
  const char *melody = get_alarm_sound_rtttl(sound_index);
  rtttl_->play(melody);
  ESP_LOGI(TAG, "Previewing alarm sound: %s", get_alarm_sound_name(sound_index));
}

void AlarmClockComponent::set_snooze_duration_option(uint8_t option_index) {
  uint8_t minutes = snooze_option_to_minutes(option_index);
  state_machine_.set_snooze_duration(minutes);
  save_settings_to_storage_();
  ui_update_snooze_selection(option_index);
  ESP_LOGI(TAG, "Snooze duration set to %d minutes", minutes);
}

void AlarmClockComponent::set_time_format_24h(bool time_format_24h) {
  time_format_24h_ = time_format_24h;
  save_settings_to_storage_();
  ui_update_time_format(time_format_24h);
  // Re-sync alarm rows and firing overlay with new format.
  sync_ui_();
  ESP_LOGI(TAG, "Time format set to %s", time_format_24h ? "24h" : "12h");
}

void AlarmClockComponent::set_pre_alarm_option(uint8_t option_index) {
  pre_alarm_minutes_ = pre_alarm_option_to_minutes(option_index);
  save_settings_to_storage_();
  ui_update_pre_alarm_selection(option_index);
  ESP_LOGI(TAG, "Pre-alarm set to %d minutes", pre_alarm_minutes_);
}

// ---------------------------------------------------------------------------
// Internal helpers.
// ---------------------------------------------------------------------------

void AlarmClockComponent::check_alarms_(uint8_t hour, uint8_t minute,
                                        uint8_t day_of_week) {
  // Store the latest time for use in loop() (e.g., snooze re-fire overlay).
  last_known_hour_ = hour;
  last_known_minute_ = minute;
  last_known_day_of_week_ = day_of_week;

  // Update the firing overlay time every second while visible.
  if (ui_is_firing_overlay_visible()) {
    ui_firing_update_time(hour, minute, time_format_24h_);
  }

  // Called every second from the YAML interval; skip if minute hasn't changed.
  if (minute == last_checked_minute_) {
    return;
  }
  last_checked_minute_ = minute;

  // Tick timers once per real-time minute (synchronized with clock source).
  // Auto-dismiss after 30 minutes of continuous firing.
  if (state_machine_.tick_firing()) {
    ESP_LOGI(TAG, "Alarm auto-dismissed after %d minutes",
             kAutoDismissMinutes);
    stop_alarm_sound_();
    ui_hide_firing_overlay();
    ui_firing_stop_animation();
    if (fired_alarm_index_ < kMaxAlarms) {
      ui_clear_alarm_row_firing(fired_alarm_index_);
    }
    auto_disable_one_shot_alarm_();
    fired_alarm_index_ = 0xFF;
    fire_ha_event_("esphome.alarm_dismissed");
    return;
  }

  if (state_machine_.tick_snooze()) {
    // Snooze expired — alarm fires again.
    start_alarm_sound_();
    ui_show_firing_overlay();
    ui_firing_start_animation();
    ui_firing_update_time(hour, minute, time_format_24h_);
    if (fired_alarm_index_ < kMaxAlarms) {
      ui_firing_update_label(alarms_[fired_alarm_index_].label);
    }
    fire_ha_event_("esphome.alarm_fired");
    return;
  }

  // Update the next-alarm display and pre-alarm banner every minute.
  update_next_alarm_display_(hour, minute, day_of_week);

  if (state_machine_.state() != AlarmState::kIdle) {
    // Already firing or snoozed — queue any matching alarms for later.
    for (uint8_t i = 0; i < kMaxAlarms; i++) {
      if (i == fired_alarm_index_) {
        continue;  // Don't re-queue the currently firing alarm.
      }
      if (alarm_matches(alarms_[i], hour, minute, day_of_week)) {
        pending_alarm_mask_ |= (1 << i);
        ESP_LOGI(TAG, "Alarm %d queued (another alarm is active)", i);
      }
    }
    return;
  }

  // Check for pending queued alarm first.
  if (pending_alarm_mask_ != 0) {
    // Find the first set bit in the pending mask.
    uint8_t pending = 0xFF;
    for (uint8_t i = 0; i < kMaxAlarms; i++) {
      if (pending_alarm_mask_ & (1 << i)) {
        pending = i;
        pending_alarm_mask_ &= ~(1 << i);
        break;
      }
    }
    // Fire the pending alarm if it's still enabled and within a
    // reasonable grace period (same hour as scheduled, i.e., at most
    // 59 minutes late).  Stale pending alarms are discarded.
    if (pending < kMaxAlarms && alarms_[pending].enabled) {
      int32_t mins_until = minutes_until_alarm(
          alarms_[pending], hour, minute, day_of_week);
      // mins_until == 0 means it matches now; a large value means it
      // wrapped around (>= 23*60 means it just passed within the hour).
      bool is_stale = (mins_until > 60 && mins_until < 23 * 60);
      if (is_stale) {
        ESP_LOGW(TAG, "Discarding stale pending alarm %d (next in %d min)",
                 pending, mins_until);
      } else {
        ESP_LOGW(TAG, "Firing queued alarm %d!", pending);
        fired_alarm_index_ = pending;
        state_machine_.trigger();
        start_alarm_sound_();
        ui_show_firing_overlay();
        ui_firing_start_animation();
        ui_firing_update_time(hour, minute, time_format_24h_);
        ui_firing_update_label(alarms_[pending].label);
        ui_set_alarm_row_firing(pending);
        fire_ha_event_("esphome.alarm_fired");
        return;
      }
    }
  }

  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    if (alarm_matches(alarms_[i], hour, minute, day_of_week)) {
      ESP_LOGW(TAG, "Alarm %d triggered!", i);
      fired_alarm_index_ = i;
      state_machine_.trigger();
      start_alarm_sound_();
      ui_show_firing_overlay();
      ui_firing_start_animation();
      ui_firing_update_time(hour, minute, time_format_24h_);
      ui_firing_update_label(alarms_[i].label);
      ui_set_alarm_row_firing(i);
      fire_ha_event_("esphome.alarm_fired");
      break;
    }
  }
}

void AlarmClockComponent::update_backlight_() {
  float bright = 0.0f;

  if (screen_asleep_) {
    // Idle mode: stay close to the configured brightness in brighter rooms,
    // but dim more aggressively when the ambient sensor says it is dark.
    float dim_amount = (sensor_factor_ >= kSleepDaySensorThreshold)
                           ? kSleepDayDimAmount
                           : kSleepNightDimAmount;
    bright = brightness_ - dim_amount;
    if (bright < kSleepBrightnessFloor) {
      bright = kSleepBrightnessFloor;
    }
  } else {
    bright = compute_brightness(brightness_, sensor_factor_);
  }

  uint8_t pwm = brightness_to_pwm(bright);
  if (pwm == last_backlight_pwm_) {
    return;
  }

  if (this->write(&pwm, 1) == ::esphome::i2c::ERROR_OK) {
    last_backlight_pwm_ = pwm;
  }
}

void AlarmClockComponent::check_screen_sleep_() {
  // Don't sleep while the alarm is firing — keep screen bright.
  if (state_machine_.state() != AlarmState::kIdle) {
    if (screen_asleep_) {
      screen_asleep_ = false;
      update_backlight_();
    }
    return;
  }

  uint32_t idle_ms = lv_disp_get_inactive_time(nullptr);
  bool should_sleep = (idle_ms >= kScreenIdleTimeoutMs);

  if (should_sleep && !screen_asleep_) {
    // Transition to sleep — dim to sensor-based minimum.
    screen_asleep_ = true;
    update_backlight_();
  } else if (!should_sleep && screen_asleep_) {
    // Wake on tap — boost back to user-configured brightness.
    screen_asleep_ = false;
    update_backlight_();
  }
}

void AlarmClockComponent::start_alarm_sound_() {
  ESP_LOGI(TAG, "Starting alarm sound (volume=%.0f%%)", volume_ * 100);
  alarm_sound_active_ = true;
  alarm_pause_active_ = false;
  // On snooze re-fire, skip the volume ramp — user is already aware.
  if (state_machine_.snooze_count() > 0) {
    alarm_sound_start_ms_ = ::esphome::millis() - kVolumeRampDurationMs;
  } else {
    alarm_sound_start_ms_ = ::esphome::millis();
  }
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
  uint32_t elapsed = ::esphome::millis() - alarm_sound_start_ms_;
  float gain = compute_ramp_volume(volume_, elapsed);
  rtttl_->set_gain(gain);
  const char *melody = get_alarm_sound_rtttl(selected_sound_index_);
  rtttl_->play(melody);
  ESP_LOGD(TAG, "Playing alarm melody '%s' (gain=%.2f, elapsed=%ums)",
           get_alarm_sound_name(selected_sound_index_), gain, elapsed);
}

void AlarmClockComponent::on_rtttl_finished() {
  if (!alarm_sound_active_) {
    return;
  }
  // Start a pause before the next melody loop.
  alarm_pause_active_ = true;
  alarm_pause_start_ms_ = ::esphome::millis();
  ESP_LOGD(TAG, "RTTTL finished, pausing %ums before next loop",
           kAlarmPauseDurationMs);
}

void AlarmClockComponent::sync_ui_() {
  // Sync UI state with component state (e.g., after loading from NVS).
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    if (alarms_[i].enabled || alarms_[i].days_of_week != 0 ||
        alarms_[i].hour != 0 || alarms_[i].minute != 0) {
      ui_update_alarm_row(i, alarms_[i].hour, alarms_[i].minute,
                          alarms_[i].days_of_week, alarms_[i].enabled,
                          time_format_24h_, alarms_[i].label);
    }
  }
  ui_update_volume(volume_);
  ui_update_brightness(brightness_);
  ui_update_sound_selection(selected_sound_index_);
  ui_update_snooze_selection(
      snooze_minutes_to_option(state_machine_.snooze_duration_minutes()));
  ui_update_time_format(time_format_24h_);
  ui_update_pre_alarm_selection(pre_alarm_minutes_to_option(pre_alarm_minutes_));
}

void AlarmClockComponent::fire_ha_event_(const char *event_type) {
#ifdef USE_API_HOMEASSISTANT_SERVICES
  auto *api = ::esphome::api::global_api_server;
  if (api == nullptr) {
    return;
  }
  ::esphome::api::HomeassistantActionRequest req;
  req.service = ::esphome::StringRef(event_type);
  req.is_event = true;
  api->send_homeassistant_action(req);
  ESP_LOGD(TAG, "Fired HA event: %s", event_type);
#else
  ESP_LOGW(TAG, "HA services not enabled, cannot fire event: %s", event_type);
#endif
}

void AlarmClockComponent::auto_disable_one_shot_alarm_() {
  if (fired_alarm_index_ >= kMaxAlarms) {
    return;
  }
  if (!is_one_shot(alarms_[fired_alarm_index_])) {
    return;
  }
  ESP_LOGI(TAG, "One-shot alarm %d auto-disabled after firing",
           fired_alarm_index_);
  alarms_[fired_alarm_index_].enabled = false;
  storage_save_alarm(fired_alarm_index_, alarms_[fired_alarm_index_]);
  ui_update_alarm_row(fired_alarm_index_,
                      alarms_[fired_alarm_index_].hour,
                      alarms_[fired_alarm_index_].minute,
                      alarms_[fired_alarm_index_].days_of_week,
                      false,
                      time_format_24h_,
                      alarms_[fired_alarm_index_].label);
}

void AlarmClockComponent::update_next_alarm_display_(uint8_t hour,
                                                     uint8_t minute,
                                                     uint8_t day_of_week) {
  int32_t minutes_until = 0;
  int8_t idx = find_next_alarm_index(alarms_, kMaxAlarms, hour, minute,
                                     day_of_week, &minutes_until);

  // Update next alarm text.
  if (idx >= 0 && minutes_until > 0) {
    char buf[64];
    format_next_alarm_text(alarms_[idx], minutes_until, time_format_24h_,
                           buf, sizeof(buf));
    ui_update_next_alarm(buf);
  } else {
    ui_update_next_alarm("");
  }

  // Update pre-alarm banner (show when within pre_alarm_minutes_).
  // Also show snooze countdown when in snoozed state.
  if (state_machine_.state() == AlarmState::kSnoozed) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Snoozed - resumes in %u min",
             state_machine_.snooze_remaining_minutes());
    ui_update_pre_alarm_banner(buf);
  } else if (idx >= 0 && minutes_until > 0 && minutes_until <= pre_alarm_minutes_ &&
      state_machine_.state() == AlarmState::kIdle) {
    char buf[48];
    format_pre_alarm_text(static_cast<uint16_t>(minutes_until),
                          alarms_[idx].label, buf, sizeof(buf));
    ui_update_pre_alarm_banner(buf);
  } else {
    ui_update_pre_alarm_banner("");
  }
}

void AlarmClockComponent::save_alarms_to_storage_() {
  for (uint8_t i = 0; i < kMaxAlarms; ++i) {
    storage_save_alarm(i, alarms_[i]);
  }
}

void AlarmClockComponent::save_settings_to_storage_() {
  StorageSettings settings;
  settings.volume = volume_;
  settings.brightness = brightness_;
  settings.snooze_duration_minutes = state_machine_.snooze_duration_minutes();
  settings.time_format_24h = time_format_24h_;
  settings.selected_sound_index = selected_sound_index_;
  settings.pre_alarm_minutes = pre_alarm_minutes_;
  storage_save_settings(settings);
}

void AlarmClockComponent::mark_settings_dirty_() {
  settings_dirty_ = true;
  settings_dirty_ms_ = ::esphome::millis();
}

void AlarmClockComponent::load_from_storage_() {
  // Load alarms.
  bool any_loaded = false;
  for (uint8_t i = 0; i < kMaxAlarms; ++i) {
    if (storage_load_alarm(i, &alarms_[i])) {
      any_loaded = true;
      ESP_LOGI(TAG, "Loaded alarm %u from NVS: %02u:%02u days=0x%02X en=%d",
               i, alarms_[i].hour, alarms_[i].minute,
               alarms_[i].days_of_week, alarms_[i].enabled);
    }
  }

  if (!any_loaded) {
    // First boot — set a default alarm and persist it.
    ESP_LOGI(TAG, "No alarms in NVS, setting default");
    alarms_[0].hour = 7;
    alarms_[0].minute = 0;
    alarms_[0].days_of_week = kWeekdays;
    alarms_[0].enabled = true;
    alarm_set_label(alarms_[0], nullptr);
    storage_save_alarm(0, alarms_[0]);
  }

  // Load settings.
  StorageSettings settings;
  if (storage_load_settings(&settings)) {
    volume_ = settings.volume;
    brightness_ = settings.brightness;
    state_machine_.set_snooze_duration(settings.snooze_duration_minutes);
    selected_sound_index_ = settings.selected_sound_index;
    time_format_24h_ = settings.time_format_24h;
    pre_alarm_minutes_ = settings.pre_alarm_minutes;
    ESP_LOGI(TAG, "Loaded settings from NVS");
  }

  // Sync UI with loaded data.
  sync_ui_();
}

}  // namespace alarmclock

#endif  // UNIT_TEST
