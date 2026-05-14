// UI manager — creates pages, handles swipe navigation, manages overlays.
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
void ui_build_firing_overlay(lv_obj_t *parent);
void ui_build_page_dots(lv_obj_t *parent);

// Called once after LVGL is initialized to build the entire UI.
void ui_init();

// Page navigation.
void ui_show_page(uint8_t page_index);
void ui_next_page();
void ui_prev_page();
uint8_t ui_current_page();

// Firing overlay control.
void ui_show_firing_overlay();
void ui_hide_firing_overlay();
bool ui_is_firing_overlay_visible();

// Update functions (called from loop/interval).
void ui_update_clock(uint8_t hour, uint8_t minute, bool time_format_24h = false);
void ui_update_date(uint8_t month, uint8_t day, uint8_t day_of_week);
void ui_update_next_alarm(const char *text);  // e.g. "Next: 7:00 AM" or ""
void ui_update_pre_alarm_banner(const char *text);  // e.g. "Alarm in 5 min — Work" or "" to hide
void ui_update_page_dots(uint8_t active_page);
void ui_update_brightness(float brightness);
void ui_update_volume(float volume);
void ui_update_sound_selection(uint8_t sound_index);
void ui_update_snooze_selection(uint8_t option_index);
void ui_update_time_format(bool time_format_24h);
void ui_update_pre_alarm_selection(uint8_t option_index);

// Alarm list management.
void ui_update_alarm_row(uint8_t index, uint8_t hour, uint8_t minute,
                         uint8_t days_mask, bool enabled,
                         bool time_format_24h = false,
                         const char *label = nullptr);
void ui_hide_alarm_row(uint8_t index);
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
  void (*on_alarm_toggle)(uint8_t index, bool enabled) = nullptr;
  void (*on_alarm_edit)(uint8_t index) = nullptr;
  void (*on_alarm_time_set)(uint8_t index, uint8_t hour, uint8_t minute) = nullptr;
  void (*on_alarm_days_set)(uint8_t index, uint8_t days_mask) = nullptr;
  void (*on_alarm_label_set)(uint8_t index, const char *label) = nullptr;
  void (*on_alarm_delete)(uint8_t index) = nullptr;
  void (*on_sound_change)(uint8_t sound_index) = nullptr;
  void (*on_snooze_duration_change)(uint8_t option_index) = nullptr;
  void (*on_time_format_change)(bool time_format_24h) = nullptr;
  void (*on_pre_alarm_change)(uint8_t option_index) = nullptr;
};

void ui_set_callbacks(const UiCallbacks &cb);

}  // namespace alarmclock

#endif  // UNIT_TEST
#endif  // UI_H
