// UI manager — creates pages, navigation chrome, and overlays.
#ifndef UI_H
#define UI_H

#ifndef UNIT_TEST

#include "lvgl.h"
#include "ui_theme.h"

namespace alarmclock {

// Forward declarations for page builders.
void ui_build_clock_page(lv_obj_t *parent);
void ui_build_alarm_page(lv_obj_t *parent);
void ui_build_settings_page(lv_obj_t *parent);
void ui_build_controls_page(lv_obj_t *parent);
void ui_build_minisplit_page(lv_obj_t *parent);
void ui_build_firing_overlay(lv_obj_t *parent);

// Called once after LVGL is initialized to build the entire UI.
void ui_init();
void ui_configure_optional_pages(bool controls_enabled,
                                 bool minisplit_enabled);
bool ui_controls_page_enabled();
bool ui_minisplit_page_enabled();

// Page navigation.
void ui_show_clock_page();
void ui_show_alarm_page();
void ui_show_settings_page();
void ui_show_controls_page();
void ui_show_minisplit_page();
void ui_show_page(uint8_t page_index);
uint8_t ui_current_page();

// Shared fixed header used by secondary pages.
void ui_create_page_header(lv_obj_t *parent, const char *title);
void ui_update_header_time(uint8_t hour, uint8_t minute,
                           bool time_format_24h);

// Firing overlay control.
void ui_show_firing_overlay();
void ui_hide_firing_overlay();
bool ui_is_firing_overlay_visible();

// Update functions (called from loop/interval).
void ui_update_clock(uint8_t hour, uint8_t minute, bool time_format_24h = false);
// Re-renders the clock label from the last known time (no time source needed).
void ui_refresh_clock(bool time_format_24h);
void ui_update_date(uint8_t month, uint8_t day, uint8_t day_of_week);
void ui_update_next_alarm(const char *text);  // e.g. "Next: 7:00 AM" or ""
void ui_update_pre_alarm_banner(const char *text);  // e.g. "Alarm in 5 min — Work" or "" to hide
void ui_set_content_brightness(float brightness);
void ui_update_brightness(float brightness);
void ui_update_min_brightness(float brightness_fraction);
void ui_update_volume(float volume);
void ui_update_sound_selection(uint8_t sound_index);
void ui_update_snooze_selection(uint8_t option_index);
void ui_update_time_format(bool time_format_24h);
void ui_update_pre_alarm_selection(uint8_t option_index);

enum class UiControlType : uint8_t {
  kToggle = 0,
  kAction = 1,
  kDimmer = 2,
};

void ui_add_control(uint8_t index, UiControlType type, const char *name);
void ui_update_control_state(uint8_t index, const char *state,
                             bool available);
void ui_update_control_brightness(uint8_t index, float brightness,
                                  bool available);

void ui_set_minisplit_options(const char *const *hvac_modes,
                              uint8_t hvac_mode_count,
                              const char *const *fan_modes,
                              uint8_t fan_mode_count,
                              float min_temperature,
                              float max_temperature,
                              float temperature_step,
                              const char *temperature_unit);
void ui_update_minisplit_state(const char *mode, const char *action,
                               float current_temperature,
                               float target_temperature, const char *fan_mode,
                               const char *horizontal_vane,
                               const char *vertical_vane, bool available);
void ui_close_minisplit_picker();

// Alarm list management.
// |now_hour|/|now_minute| are the current time-of-day, used to label a
// one-shot alarm as "Today"/"Tomorrow"/"Not Scheduled" instead of "Once".
void ui_update_alarm_row(uint8_t index, uint8_t hour, uint8_t minute,
                         uint8_t days_mask, bool enabled,
                         bool time_format_24h = false,
                         const char *label = nullptr,
                         bool skip_next = false,
                         uint8_t now_hour = 0, uint8_t now_minute = 0);
void ui_hide_alarm_row(uint8_t index);
void ui_set_alarm_slots_used(uint8_t used_slots, uint8_t max_slots);
void ui_set_alarm_row_firing(uint8_t index);
void ui_clear_alarm_row_firing(uint8_t index);

// Time picker (alarm edit overlay).
void ui_build_time_picker(lv_obj_t *parent);
void ui_show_time_picker(uint8_t alarm_index, uint8_t hour, uint8_t minute,
                         uint8_t days_mask, const char *label,
                         bool time_format_24h = false);
void ui_hide_time_picker();

// Firing overlay animation.
void ui_firing_update_time(uint8_t hour, uint8_t minute,
                           bool time_format_24h = false);
void ui_firing_update_label(const char *label);
void ui_firing_start_animation();
void ui_firing_stop_animation();

// Callbacks the component registers to get notified of UI events.
// Set these before calling ui_init().
struct UiCallbacks {
  void (*on_alarm_dismiss)() = nullptr;
  void (*on_alarm_snooze)() = nullptr;
  void (*on_volume_change)(float volume) = nullptr;
  void (*on_brightness_change)(float brightness) = nullptr;
  void (*on_min_brightness_change)(float brightness_fraction) = nullptr;
  void (*on_alarm_add)() = nullptr;
  void (*on_alarm_toggle)(uint8_t index, bool enabled) = nullptr;
  void (*on_alarm_edit)(uint8_t index) = nullptr;
  void (*on_alarm_save)(uint8_t index, uint8_t hour, uint8_t minute,
                        uint8_t days_mask, const char *label) = nullptr;
  void (*on_alarm_delete)(uint8_t index) = nullptr;
  void (*on_alarm_skip_toggle)(uint8_t index, bool skip) = nullptr;
  void (*on_sound_change)(uint8_t sound_index) = nullptr;
  void (*on_sound_preview)(uint8_t sound_index) = nullptr;
  void (*on_sound_preview_cancel)() = nullptr;
  void (*on_snooze_duration_change)(uint8_t option_index) = nullptr;
  void (*on_time_format_change)(bool time_format_24h) = nullptr;
  void (*on_pre_alarm_change)(uint8_t option_index) = nullptr;
  void (*on_control_press)(uint8_t index) = nullptr;
  void (*on_control_brightness_change)(uint8_t index, float brightness) =
      nullptr;
  void (*on_minisplit_power)() = nullptr;
  void (*on_minisplit_temperature_change)(float delta) = nullptr;
  void (*on_minisplit_hvac_mode)(const char *mode) = nullptr;
  void (*on_minisplit_fan_mode)(const char *mode) = nullptr;
  void (*on_minisplit_vane_step)(bool horizontal, bool next) = nullptr;
};

void ui_set_callbacks(const UiCallbacks &cb);

}  // namespace alarmclock

#endif  // UNIT_TEST
#endif  // UI_H
