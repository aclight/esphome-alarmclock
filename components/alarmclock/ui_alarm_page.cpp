// Alarm list/edit page — view alarms, toggle, edit time and days.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_theme.h"
#include "alarmclock.h"
#include "lvgl.h"
#include <cstdio>

namespace alarmclock {

// ---------------------------------------------------------------------------
// Constants.
// ---------------------------------------------------------------------------
static constexpr int16_t kAlarmRowHeight = 110;
static constexpr int16_t kAlarmRowGap = 6;
static constexpr int16_t kAlarmListStartY = 50;

// ---------------------------------------------------------------------------
// Per-alarm row widgets.
// ---------------------------------------------------------------------------
struct AlarmRow {
  lv_obj_t *container = nullptr;
  lv_obj_t *time_label = nullptr;
  lv_obj_t *alarm_label = nullptr;
  lv_obj_t *days_label = nullptr;
  lv_obj_t *toggle = nullptr;
};

static AlarmRow alarm_rows_[kMaxAlarms] = {};
static lv_obj_t *title_label_ = nullptr;
static lv_obj_t *add_btn_ = nullptr;

// Forward declaration.
const UiCallbacks &ui_get_callbacks();

// ---------------------------------------------------------------------------
// Event handlers.
// ---------------------------------------------------------------------------
static void alarm_toggle_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  lv_obj_t *toggle = static_cast<lv_obj_t *>(lv_event_get_target(e));
  bool enabled = lv_obj_has_state(toggle, LV_STATE_CHECKED);
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_toggle) {
    cb.on_alarm_toggle(index, enabled);
  }
}

static void alarm_row_click_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_edit) {
    cb.on_alarm_edit(index);
  }
}

static void add_btn_cb(lv_event_t *e) {
  (void)e;
  const auto &cb = ui_get_callbacks();
  // Find the first unused alarm slot (all fields at default).
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    if (lv_obj_has_flag(alarm_rows_[i].container, LV_OBJ_FLAG_HIDDEN)) {
      // Open time picker for this empty slot with defaults.
      if (cb.on_alarm_edit) {
        cb.on_alarm_edit(i);
      }
      return;
    }
  }
  // All slots full — no action (button could be disabled in the future).
}

// ---------------------------------------------------------------------------
// Build the alarm page.
// ---------------------------------------------------------------------------
void ui_build_alarm_page(lv_obj_t *parent) {
  // Title.
  title_label_ = lv_label_create(parent);
  lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(title_label_, "Alarms");

  // Create alarm rows.
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    AlarmRow &row = alarm_rows_[i];

    // Row container.
    row.container = lv_obj_create(parent);
    lv_obj_set_size(row.container, theme::kScreenWidth - 40, kAlarmRowHeight);
    lv_obj_align(row.container, LV_ALIGN_TOP_MID, 0,
                 kAlarmListStartY + i * (kAlarmRowHeight + kAlarmRowGap));
    lv_obj_set_style_bg_color(row.container, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(row.container, 0, 0);
    lv_obj_set_style_radius(row.container, 10, 0);
    lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row.container, alarm_row_click_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    // Time label (e.g. "7:00 AM").
    row.time_label = lv_label_create(row.container);
    lv_obj_align(row.time_label, LV_ALIGN_LEFT_MID, 15, -18);
    lv_obj_set_style_text_font(row.time_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(row.time_label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_text(row.time_label, "--:--");

    // Alarm label (e.g. "Work").
    row.alarm_label = lv_label_create(row.container);
    lv_obj_align(row.alarm_label, LV_ALIGN_LEFT_MID, 15, 12);
    lv_obj_set_style_text_font(row.alarm_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(row.alarm_label, lv_color_hex(theme::kColorAccent), 0);
    lv_label_set_text(row.alarm_label, "");

    // Days label (e.g. "Mon Tue Wed Thu Fri").
    row.days_label = lv_label_create(row.container);
    lv_obj_align(row.days_label, LV_ALIGN_LEFT_MID, 15, 30);
    lv_obj_set_style_text_font(row.days_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(row.days_label, lv_color_hex(theme::kColorSecondary), 0);
    lv_label_set_text(row.days_label, "");

    // Toggle switch.
    row.toggle = lv_switch_create(row.container);
    lv_obj_align(row.toggle, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_size(row.toggle, 64, 34);
    lv_obj_add_event_cb(row.toggle, alarm_toggle_cb, LV_EVENT_VALUE_CHANGED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    // Initially hide all rows (shown when alarms are configured).
    lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);
  }

  // "Add alarm" button.
  add_btn_ = lv_button_create(parent);
  lv_obj_align(add_btn_, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_size(add_btn_, 160, 50);
  lv_obj_set_style_radius(add_btn_, theme::kButtonRadius, 0);
  lv_obj_set_style_bg_color(add_btn_, lv_color_hex(theme::kColorAccent), 0);
  lv_obj_add_event_cb(add_btn_, add_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *add_label = lv_label_create(add_btn_);
  lv_obj_center(add_label);
  lv_obj_set_style_text_color(add_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(add_label, "+ Add Alarm");
}

// ---------------------------------------------------------------------------
// Public: update alarm row display.
// ---------------------------------------------------------------------------

// Call this to show/update an alarm in the list.
// days_mask uses DayOfWeek flags from alarm_time.h.
void ui_update_alarm_row(uint8_t index, uint8_t hour, uint8_t minute,
                         uint8_t days_mask, bool enabled,
                         bool time_format_24h,
                         const char *label) {
  if (index >= kMaxAlarms) {
    return;
  }
  AlarmRow &row = alarm_rows_[index];
  lv_obj_clear_flag(row.container, LV_OBJ_FLAG_HIDDEN);

  // Format time using the user's preferred format.
  char buf[12];
  format_clock_time(hour, minute, time_format_24h, buf, sizeof(buf));
  lv_label_set_text(row.time_label, buf);

  // Show alarm label (e.g. "Work").
  if (label != nullptr && label[0] != '\0') {
    lv_label_set_text(row.alarm_label, label);
  } else {
    lv_label_set_text(row.alarm_label, "");
  }

  // Format days.
  static const char *kShortDays[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  if (days_mask == 0) {
    // One-shot alarm — show "Once" indicator.
    lv_label_set_text(row.days_label, "Once");
  } else {
    char days_buf[22] = "";
    int pos = 0;
    for (uint8_t d = 0; d < 7; d++) {
      if (days_mask & (1 << d)) {
        if (pos > 0) {
          days_buf[pos++] = ' ';
        }
        days_buf[pos++] = kShortDays[d][0];
        days_buf[pos++] = kShortDays[d][1];
      }
    }
    days_buf[pos] = '\0';
    lv_label_set_text(row.days_label, days_buf);
  }

  // Toggle state.
  if (enabled) {
    lv_obj_add_state(row.toggle, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(row.toggle, LV_STATE_CHECKED);
  }
}

// Hide an alarm row (e.g. when deleted).
void ui_hide_alarm_row(uint8_t index) {
  if (index >= kMaxAlarms) {
    return;
  }
  lv_obj_add_flag(alarm_rows_[index].container, LV_OBJ_FLAG_HIDDEN);
}

// Highlight an alarm row to indicate it is currently firing.
void ui_set_alarm_row_firing(uint8_t index) {
  if (index >= kMaxAlarms) {
    return;
  }
  lv_obj_set_style_bg_color(alarm_rows_[index].container,
                            lv_color_hex(0x331111), 0);
  lv_obj_set_style_border_width(alarm_rows_[index].container, 2, 0);
  lv_obj_set_style_border_color(alarm_rows_[index].container,
                                lv_color_hex(theme::kColorAlarmFiring), 0);
}

// Clear the firing highlight on an alarm row.
void ui_clear_alarm_row_firing(uint8_t index) {
  if (index >= kMaxAlarms) {
    return;
  }
  lv_obj_set_style_bg_color(alarm_rows_[index].container,
                            lv_color_hex(0x111111), 0);
  lv_obj_set_style_border_width(alarm_rows_[index].container, 0, 0);
}

}  // namespace alarmclock

#endif  // UNIT_TEST
