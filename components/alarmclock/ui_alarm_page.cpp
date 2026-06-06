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
static constexpr int16_t kAlarmRowHeight = 160;
static constexpr int16_t kAlarmRowGap = 8;
static constexpr int16_t kAlarmListStartY = 70;
static constexpr int16_t kRowTapDragThresholdPx = 10;

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

struct RowTouchState {
  int16_t press_x = 0;
  int16_t press_y = 0;
  bool moved = false;
};

static AlarmRow alarm_rows_[kMaxAlarms] = {};
static RowTouchState row_touch_states_[kMaxAlarms] = {};
static lv_obj_t *title_label_ = nullptr;
static lv_obj_t *add_btn_ = nullptr;
static lv_obj_t *add_btn_label_ = nullptr;
static uint8_t used_alarm_slots_ = 0;

// Forward declaration.
const UiCallbacks &ui_get_callbacks();

static bool point_from_event(lv_event_t *e, lv_point_t *out_point) {
  lv_indev_t *indev = lv_event_get_indev(e);
  if (indev == nullptr || out_point == nullptr) {
    return false;
  }
  lv_indev_get_point(indev, out_point);
  return true;
}

static void refresh_add_alarm_button_() {
  if (add_btn_ == nullptr || add_btn_label_ == nullptr) {
    return;
  }

  if (used_alarm_slots_ >= kMaxAlarms) {
    lv_obj_add_state(add_btn_, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(add_btn_, lv_color_hex(theme::kColorMuted), 0);
    lv_obj_set_style_text_font(add_btn_label_, &lv_font_montserrat_24, 0);
    char label_buf[20];
    snprintf(label_buf, sizeof(label_buf), "Max %u Alarms",
             static_cast<unsigned>(kMaxAlarms));
    lv_label_set_text(add_btn_label_, label_buf);
  } else {
    lv_obj_clear_state(add_btn_, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(add_btn_, lv_color_hex(theme::kColorAccent), 0);
    lv_obj_set_style_text_font(add_btn_label_, &lv_font_montserrat_28, 0);
    lv_label_set_text(add_btn_label_, "+ Add Alarm");
  }
}

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

static void alarm_row_pressed_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (index >= kMaxAlarms) {
    return;
  }
  lv_point_t p;
  if (point_from_event(e, &p)) {
    row_touch_states_[index].press_x = p.x;
    row_touch_states_[index].press_y = p.y;
  }
  row_touch_states_[index].moved = false;
}

static void alarm_row_pressing_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (index >= kMaxAlarms || row_touch_states_[index].moved) {
    return;
  }
  lv_point_t p;
  if (!point_from_event(e, &p)) {
    return;
  }

  int16_t dx = p.x - row_touch_states_[index].press_x;
  if (dx < 0) {
    dx = -dx;
  }
  int16_t dy = p.y - row_touch_states_[index].press_y;
  if (dy < 0) {
    dy = -dy;
  }

  if (dx > kRowTapDragThresholdPx || dy > kRowTapDragThresholdPx) {
    row_touch_states_[index].moved = true;
  }
}

static void alarm_row_released_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (index >= kMaxAlarms || row_touch_states_[index].moved) {
    return;
  }
  lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
  if (target != alarm_rows_[index].container) {
    return;
  }
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_edit) {
    cb.on_alarm_edit(index);
  }
}

static void add_btn_cb(lv_event_t *e) {
  (void)e;
  if (add_btn_ != nullptr && lv_obj_has_state(add_btn_, LV_STATE_DISABLED)) {
    return;
  }
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_add) {
    cb.on_alarm_add();
  }
}

static void home_btn_cb(lv_event_t *e) {
  (void)e;
  ui_show_clock_page();
}

// ---------------------------------------------------------------------------
// Build the alarm page.
// ---------------------------------------------------------------------------
void ui_build_alarm_page(lv_obj_t *parent) {
  // Layout: parent is flex column with fixed header + scrollable content area.
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(parent, 0, 0);
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

  // --- Fixed header row (title + home button, stays visible at top) ---
  lv_obj_t *header_row = lv_obj_create(parent);
  lv_obj_set_size(header_row, theme::kScreenWidth, 80);
  lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(header_row, 0, 0);
  lv_obj_set_style_pad_all(header_row, 10, 0);
  lv_obj_clear_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);

  title_label_ = lv_label_create(header_row);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(title_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(title_label_, "Alarms");

  lv_obj_t *home_btn = lv_button_create(header_row);
  lv_obj_set_size(home_btn, theme::kNavButtonWidth, theme::kNavButtonHeight);
  lv_obj_set_style_bg_color(home_btn, lv_color_hex(theme::kColorAccent), 0);
  lv_obj_set_style_radius(home_btn, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(home_btn, home_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *home_label = lv_label_create(home_btn);
  lv_obj_center(home_label);
  lv_obj_set_style_text_font(home_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(home_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(home_label, LV_SYMBOL_HOME);

  // --- Scrollable content area (alarm rows + add button) ---
  lv_obj_t *scroll_area = lv_obj_create(parent);
  lv_obj_set_size(scroll_area, theme::kScreenWidth, theme::kScreenHeight - 80);
  lv_obj_set_flex_flow(scroll_area, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scroll_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(scroll_area, 10, 0);
  lv_obj_set_style_pad_bottom(scroll_area, 20, 0);
  lv_obj_set_style_pad_row(scroll_area, kAlarmRowGap, 0);
  lv_obj_add_flag(scroll_area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(scroll_area, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scroll_dir(scroll_area, LV_DIR_VER);
  lv_obj_set_style_bg_opa(scroll_area, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(scroll_area, 0, 0);

  // Create alarm rows (children of scroll_area, not parent).
  for (uint8_t i = 0; i < kMaxAlarms; i++) {
    AlarmRow &row = alarm_rows_[i];

    // Row container.
    row.container = lv_obj_create(scroll_area);
    lv_obj_set_size(row.container, theme::kScreenWidth - 40, kAlarmRowHeight);
    lv_obj_set_style_bg_color(row.container, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(row.container, 0, 0);
    lv_obj_set_style_radius(row.container, 10, 0);
    lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row.container, alarm_row_pressed_cb, LV_EVENT_PRESSED,
              reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
    lv_obj_add_event_cb(row.container, alarm_row_pressing_cb, LV_EVENT_PRESSING,
              reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
    lv_obj_add_event_cb(row.container, alarm_row_released_cb, LV_EVENT_RELEASED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    // Time label (e.g. "7:00 AM").
    row.time_label = lv_label_create(row.container);
    lv_obj_align(row.time_label, LV_ALIGN_LEFT_MID, 20, -28);
    lv_obj_set_style_text_font(row.time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(row.time_label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_text(row.time_label, "--:--");

    // Alarm label (e.g. "Work").
    row.alarm_label = lv_label_create(row.container);
    lv_obj_align(row.alarm_label, LV_ALIGN_LEFT_MID, 20, 18);
    lv_obj_set_style_text_font(row.alarm_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(row.alarm_label, lv_color_hex(theme::kColorAccent), 0);
    lv_label_set_text(row.alarm_label, "");

    // Days label (e.g. "Mon Tue Wed Thu Fri").
    row.days_label = lv_label_create(row.container);
    lv_obj_align(row.days_label, LV_ALIGN_LEFT_MID, 20, 48);
    lv_obj_set_style_text_font(row.days_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(row.days_label, lv_color_hex(theme::kColorSecondary), 0);
    lv_label_set_text(row.days_label, "");

    // Toggle switch.
    row.toggle = lv_switch_create(row.container);
    lv_obj_align(row.toggle, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_size(row.toggle, 90, 48);
    lv_obj_add_event_cb(row.toggle, alarm_toggle_cb, LV_EVENT_VALUE_CHANGED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    // Initially hide all rows (shown when alarms are configured).
    lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);
  }

  // "Add alarm" button (child of scroll_area so it scrolls with content).
  add_btn_ = lv_button_create(scroll_area);
  lv_obj_set_size(add_btn_, 280, 80);
  lv_obj_set_style_radius(add_btn_, theme::kButtonRadius, 0);
  lv_obj_set_style_bg_color(add_btn_, lv_color_hex(theme::kColorAccent), 0);
  lv_obj_add_event_cb(add_btn_, add_btn_cb, LV_EVENT_CLICKED, nullptr);

  add_btn_label_ = lv_label_create(add_btn_);
  lv_obj_center(add_btn_label_);
  lv_obj_set_style_text_font(add_btn_label_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(add_btn_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(add_btn_label_, "+ Add Alarm");
  refresh_add_alarm_button_();
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

void ui_set_alarm_slots_used(uint8_t used_slots, uint8_t max_slots) {
  (void)max_slots;
  used_alarm_slots_ = used_slots;
  refresh_add_alarm_button_();
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
