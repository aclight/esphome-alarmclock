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
void ui_update_clock(uint8_t hour, uint8_t minute);
void ui_update_date(uint8_t month, uint8_t day, uint8_t day_of_week);
void ui_update_next_alarm(const char *text);  // e.g. "Next: 7:00 AM" or ""
void ui_update_brightness(float brightness);
void ui_update_volume(float volume);

// Alarm list management.
void ui_update_alarm_row(uint8_t index, uint8_t hour, uint8_t minute,
                         uint8_t days_mask, bool enabled,
                         const char *label = nullptr);
void ui_hide_alarm_row(uint8_t index);

// Firing overlay animation.
void ui_firing_update_time(uint8_t hour, uint8_t minute);
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
  void (*on_alarm_time_set)(uint8_t index, uint8_t hour, uint8_t minute) = nullptr;
  void (*on_alarm_days_set)(uint8_t index, uint8_t days_mask) = nullptr;
};

void ui_set_callbacks(const UiCallbacks &cb);

}  // namespace alarmclock

#endif  // UNIT_TEST
#endif  // UI_H
