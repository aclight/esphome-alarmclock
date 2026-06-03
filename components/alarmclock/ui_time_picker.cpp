// Time picker overlay — hour/minute rollers, AM/PM toggle, day selector, label.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_theme.h"
#include "alarmclock.h"
#include "lvgl.h"
#include <cstdio>
#include <cstring>

namespace alarmclock {

// ---------------------------------------------------------------------------
// Constants.
// ---------------------------------------------------------------------------
static constexpr int16_t kPickerWidth = 700;
static constexpr int16_t kPickerHeight = 440;
static constexpr int16_t kRollerWidth = 80;
static constexpr int16_t kRollerHeight = 150;
static constexpr int16_t kDayBtnSize = 64;
static constexpr uint8_t kMaxLabelLen = 15;

// Day abbreviations (two-letter to distinguish Su/Sa and Tu/Th).
static const char *kDayLetters[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

// ---------------------------------------------------------------------------
// Static widgets.
// ---------------------------------------------------------------------------
static lv_obj_t *picker_overlay_ = nullptr;
static lv_obj_t *picker_panel_ = nullptr;
static lv_obj_t *hour_roller_ = nullptr;
static lv_obj_t *minute_roller_ = nullptr;
static lv_obj_t *ampm_roller_ = nullptr;
static lv_obj_t *day_btns_[7] = {};
static lv_obj_t *label_input_ = nullptr;
static lv_obj_t *confirm_btn_ = nullptr;
static lv_obj_t *cancel_btn_ = nullptr;
static lv_obj_t *delete_btn_ = nullptr;
static lv_obj_t *delete_confirm_overlay_ = nullptr;
static lv_obj_t *keyboard_ = nullptr;

// Currently editing alarm index.
static uint8_t editing_index_ = 0;

// Whether the time picker is currently using 24h format.
static bool picker_24h_mode_ = false;

// Forward declaration.
const UiCallbacks &ui_get_callbacks();

// ---------------------------------------------------------------------------
// Helper: build roller option strings.
// ---------------------------------------------------------------------------

// Hour options: "1\n2\n3\n...\n12"
static char hour_opts_12h_[36];  // "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12"
static char hour_opts_24h_[72];  // "00\n01\n02\n...\n23"
static char minute_opts_[180];  // "00\n01\n...\n59"

static void build_roller_options() {
  // Hours 1-12 (12h mode).
  int pos = 0;
  for (uint8_t h = 1; h <= 12; h++) {
    if (h > 1) {
      hour_opts_12h_[pos++] = '\n';
    }
    if (h >= 10) {
      hour_opts_12h_[pos++] = '1';
      hour_opts_12h_[pos++] = '0' + (h - 10);
    } else {
      hour_opts_12h_[pos++] = '0' + h;
    }
  }
  hour_opts_12h_[pos] = '\0';

  // Hours 00-23 (24h mode).
  pos = 0;
  for (uint8_t h = 0; h < 24; h++) {
    if (h > 0) {
      hour_opts_24h_[pos++] = '\n';
    }
    hour_opts_24h_[pos++] = '0' + (h / 10);
    hour_opts_24h_[pos++] = '0' + (h % 10);
  }
  hour_opts_24h_[pos] = '\0';

  // Minutes 00-59.
  pos = 0;
  for (uint8_t m = 0; m < 60; m++) {
    if (m > 0) {
      minute_opts_[pos++] = '\n';
    }
    minute_opts_[pos++] = '0' + (m / 10);
    minute_opts_[pos++] = '0' + (m % 10);
  }
  minute_opts_[pos] = '\0';
}

// ---------------------------------------------------------------------------
// Event handlers.
// ---------------------------------------------------------------------------

static void confirm_btn_cb(lv_event_t *e) {
  (void)e;
  const auto &cb = ui_get_callbacks();

  uint8_t hour_24;
  if (picker_24h_mode_) {
    // 24h mode: roller index 0 = hour 0, index 23 = hour 23.
    uint16_t hour_sel = lv_roller_get_selected(hour_roller_);
    hour_24 = static_cast<uint8_t>(hour_sel);
  } else {
    // 12h mode: roller index 0 = hour 1, index 11 = hour 12.
    uint16_t hour_sel = lv_roller_get_selected(hour_roller_);
    uint8_t hour_12 = static_cast<uint8_t>(hour_sel + 1);

    // Read AM/PM roller (index 0 = AM, index 1 = PM).
    uint16_t ampm_sel = lv_roller_get_selected(ampm_roller_);
    bool is_pm = (ampm_sel == 1);

    // Convert to 24-hour.
    hour_24 = hour_12_to_24(hour_12, is_pm);
  }

  // Read minute roller.
  uint16_t minute = lv_roller_get_selected(minute_roller_);

  // Read days.
  uint8_t days_mask = 0;
  for (uint8_t d = 0; d < 7; d++) {
    if (lv_obj_has_state(day_btns_[d], LV_STATE_CHECKED)) {
      days_mask |= (1 << d);
    }
  }

  // Read label.
  const char *label_text = lv_textarea_get_text(label_input_);

  // Fire callbacks.
  if (cb.on_alarm_time_set) {
    cb.on_alarm_time_set(editing_index_, hour_24, static_cast<uint8_t>(minute));
  }
  if (cb.on_alarm_days_set) {
    cb.on_alarm_days_set(editing_index_, days_mask);
  }
  if (cb.on_alarm_label_set) {
    cb.on_alarm_label_set(editing_index_, label_text);
  }

  ui_hide_time_picker();
}

static void delete_btn_cb(lv_event_t *e) {
  (void)e;
  // Show delete confirmation.
  if (delete_confirm_overlay_) {
    lv_obj_clear_flag(delete_confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void delete_confirm_yes_cb(lv_event_t *e) {
  (void)e;
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_delete) {
    cb.on_alarm_delete(editing_index_);
  }
  if (delete_confirm_overlay_) {
    lv_obj_add_flag(delete_confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
  ui_hide_time_picker();
}

static void delete_confirm_no_cb(lv_event_t *e) {
  (void)e;
  if (delete_confirm_overlay_) {
    lv_obj_add_flag(delete_confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void picker_backdrop_cb(lv_event_t *e) {
  lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
  // Only dismiss if clicking the backdrop itself, not child widgets.
  if (target == picker_overlay_) {
    ui_hide_time_picker();
  }
}

static void cancel_btn_cb(lv_event_t *e) {
  (void)e;
  ui_hide_time_picker();
}

static void label_focus_cb(lv_event_t *e) {
  (void)e;
  if (keyboard_) {
    lv_keyboard_set_textarea(keyboard_, label_input_);
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void label_defocus_cb(lv_event_t *e) {
  (void)e;
  // Dismiss keyboard when the textarea loses focus (e.g., tap outside).
  if (keyboard_) {
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void keyboard_ready_cb(lv_event_t *e) {
  (void)e;
  if (keyboard_) {
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void day_btn_cb(lv_event_t *e) {
  // LV_OBJ_FLAG_CHECKABLE handles the toggle automatically before this
  // callback fires.  No manual state manipulation needed.
  (void)e;
}

// ---------------------------------------------------------------------------
// Build the delete confirmation sub-overlay.
// ---------------------------------------------------------------------------
static void build_delete_confirm(lv_obj_t *parent) {
  delete_confirm_overlay_ = lv_obj_create(parent);
  lv_obj_set_size(delete_confirm_overlay_, kPickerWidth, kPickerHeight);
  lv_obj_center(delete_confirm_overlay_);
  lv_obj_set_style_bg_color(delete_confirm_overlay_,
                            lv_color_hex(theme::kColorBackground), 0);
  lv_obj_set_style_bg_opa(delete_confirm_overlay_, LV_OPA_90, 0);
  lv_obj_set_style_border_width(delete_confirm_overlay_, 0, 0);
  lv_obj_set_style_radius(delete_confirm_overlay_, 12, 0);
  lv_obj_clear_flag(delete_confirm_overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(delete_confirm_overlay_, LV_OBJ_FLAG_HIDDEN);

  // "Delete this alarm?" label.
  lv_obj_t *msg = lv_label_create(delete_confirm_overlay_);
  lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_text_font(msg, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(msg, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(msg, "Delete this alarm?");

  // Yes button.
  lv_obj_t *yes_btn = lv_button_create(delete_confirm_overlay_);
  lv_obj_set_size(yes_btn, 120, 50);
  lv_obj_align(yes_btn, LV_ALIGN_BOTTOM_LEFT, 80, -60);
  lv_obj_set_style_bg_color(yes_btn, lv_color_hex(theme::kColorAlarmFiring), 0);
  lv_obj_set_style_radius(yes_btn, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(yes_btn, delete_confirm_yes_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *yes_label = lv_label_create(yes_btn);
  lv_obj_center(yes_label);
  lv_obj_set_style_text_color(yes_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(yes_label, "Delete");

  // No button.
  lv_obj_t *no_btn = lv_button_create(delete_confirm_overlay_);
  lv_obj_set_size(no_btn, 120, 50);
  lv_obj_align(no_btn, LV_ALIGN_BOTTOM_RIGHT, -80, -60);
  lv_obj_set_style_bg_color(no_btn, lv_color_hex(theme::kColorMuted), 0);
  lv_obj_set_style_radius(no_btn, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(no_btn, delete_confirm_no_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *no_label = lv_label_create(no_btn);
  lv_obj_center(no_label);
  lv_obj_set_style_text_color(no_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(no_label, "Cancel");
}

// ---------------------------------------------------------------------------
// Build the time picker overlay (called once from ui_init).
// ---------------------------------------------------------------------------
void ui_build_time_picker(lv_obj_t *parent) {
  build_roller_options();

  picker_overlay_ = lv_obj_create(parent);
  lv_obj_set_size(picker_overlay_, theme::kScreenWidth, theme::kScreenHeight);
  lv_obj_align(picker_overlay_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(picker_overlay_, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(picker_overlay_, LV_OPA_80, 0);
  lv_obj_set_style_border_width(picker_overlay_, 0, 0);
  lv_obj_set_style_radius(picker_overlay_, 0, 0);
  lv_obj_clear_flag(picker_overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(picker_overlay_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(picker_overlay_, picker_backdrop_cb,
                      LV_EVENT_CLICKED, nullptr);

  // Inner panel (the actual picker card).
  picker_panel_ = lv_obj_create(picker_overlay_);
  lv_obj_set_size(picker_panel_, kPickerWidth, kPickerHeight);
  lv_obj_center(picker_panel_);
  lv_obj_set_style_bg_color(picker_panel_, lv_color_hex(0x1A1A1A), 0);
  lv_obj_set_style_border_width(picker_panel_, 1, 0);
  lv_obj_set_style_border_color(picker_panel_, lv_color_hex(theme::kColorDivider), 0);
  lv_obj_set_style_radius(picker_panel_, 16, 0);
  lv_obj_set_style_pad_all(picker_panel_, 20, 0);
  lv_obj_clear_flag(picker_panel_, LV_OBJ_FLAG_SCROLLABLE);

  // --- Hour roller ---
  hour_roller_ = lv_roller_create(picker_panel_);
  lv_roller_set_options(hour_roller_, hour_opts_12h_, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(hour_roller_, 3);
  lv_obj_set_size(hour_roller_, kRollerWidth, kRollerHeight);
  lv_obj_align(hour_roller_, LV_ALIGN_TOP_LEFT, 30, 10);
  lv_obj_set_style_text_font(hour_roller_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(hour_roller_, lv_color_hex(theme::kColorPrimary), 0);
  lv_obj_set_style_bg_color(hour_roller_, lv_color_hex(0x222222), 0);
  lv_obj_set_style_bg_color(hour_roller_, lv_color_hex(theme::kColorAccent),
                            LV_PART_SELECTED);

  // Colon label between hour and minute.
  lv_obj_t *colon = lv_label_create(picker_panel_);
  lv_obj_align(colon, LV_ALIGN_TOP_LEFT, 120, 65);
  lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(colon, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(colon, ":");

  // --- Minute roller ---
  minute_roller_ = lv_roller_create(picker_panel_);
  lv_roller_set_options(minute_roller_, minute_opts_, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(minute_roller_, 3);
  lv_obj_set_size(minute_roller_, kRollerWidth, kRollerHeight);
  lv_obj_align(minute_roller_, LV_ALIGN_TOP_LEFT, 140, 10);
  lv_obj_set_style_text_font(minute_roller_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(minute_roller_, lv_color_hex(theme::kColorPrimary), 0);
  lv_obj_set_style_bg_color(minute_roller_, lv_color_hex(0x222222), 0);
  lv_obj_set_style_bg_color(minute_roller_, lv_color_hex(theme::kColorAccent),
                            LV_PART_SELECTED);

  // --- AM/PM roller ---
  ampm_roller_ = lv_roller_create(picker_panel_);
  lv_roller_set_options(ampm_roller_, "AM\nPM", LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(ampm_roller_, 2);
  lv_obj_set_size(ampm_roller_, 70, 80);
  lv_obj_align(ampm_roller_, LV_ALIGN_TOP_LEFT, 240, 45);
  lv_obj_set_style_text_font(ampm_roller_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ampm_roller_, lv_color_hex(theme::kColorPrimary), 0);
  lv_obj_set_style_bg_color(ampm_roller_, lv_color_hex(0x222222), 0);
  lv_obj_set_style_bg_color(ampm_roller_, lv_color_hex(theme::kColorAccent),
                            LV_PART_SELECTED);

  // --- Day-of-week toggle buttons ---
  lv_obj_t *days_label = lv_label_create(picker_panel_);
  lv_obj_align(days_label, LV_ALIGN_TOP_LEFT, 30, 175);
  lv_obj_set_style_text_font(days_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(days_label, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(days_label, "Repeat:");

  for (uint8_t d = 0; d < 7; d++) {
    day_btns_[d] = lv_button_create(picker_panel_);
    lv_obj_set_size(day_btns_[d], kDayBtnSize, kDayBtnSize);
    lv_obj_align(day_btns_[d], LV_ALIGN_TOP_LEFT,
                 30 + d * (kDayBtnSize + 8), 210);
    lv_obj_set_style_radius(day_btns_[d], kDayBtnSize / 2, 0);
    lv_obj_set_style_bg_color(day_btns_[d], lv_color_hex(theme::kColorMuted), 0);
    lv_obj_set_style_bg_color(day_btns_[d], lv_color_hex(theme::kColorAccent),
                              LV_STATE_CHECKED);
    lv_obj_add_flag(day_btns_[d], LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(day_btns_[d], day_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btn_label = lv_label_create(day_btns_[d]);
    lv_obj_center(btn_label);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_text(btn_label, kDayLetters[d]);
  }

  // --- Label input (upper-right area, above the keyboard when visible) ---
  lv_obj_t *label_title = lv_label_create(picker_panel_);
  lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 370, 15);
  lv_obj_set_style_text_font(label_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(label_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(label_title, "Label:");

  label_input_ = lv_textarea_create(picker_panel_);
  lv_obj_set_size(label_input_, 280, 48);
  lv_obj_align(label_input_, LV_ALIGN_TOP_LEFT, 370, 50);
  lv_textarea_set_max_length(label_input_, kMaxLabelLen);
  lv_textarea_set_one_line(label_input_, true);
  lv_textarea_set_placeholder_text(label_input_, "Alarm label");
  lv_obj_set_style_text_font(label_input_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(label_input_, lv_color_hex(theme::kColorPrimary), 0);
  lv_obj_set_style_bg_color(label_input_, lv_color_hex(0x222222), 0);
  lv_obj_set_style_border_color(label_input_,
                                lv_color_hex(theme::kColorAccent), 0);

  // --- Confirm button ---
  confirm_btn_ = lv_button_create(picker_panel_);
  lv_obj_set_size(confirm_btn_, 150, 60);
  lv_obj_align(confirm_btn_, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_set_style_bg_color(confirm_btn_, lv_color_hex(theme::kColorAccent), 0);
  lv_obj_set_style_radius(confirm_btn_, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(confirm_btn_, confirm_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *confirm_label = lv_label_create(confirm_btn_);
  lv_obj_center(confirm_label);
  lv_obj_set_style_text_font(confirm_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(confirm_label,
                              lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(confirm_label, "Save");

  // --- Cancel button ---
  cancel_btn_ = lv_button_create(picker_panel_);
  lv_obj_set_size(cancel_btn_, 150, 60);
  lv_obj_align(cancel_btn_, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(cancel_btn_, lv_color_hex(theme::kColorMuted), 0);
  lv_obj_set_style_radius(cancel_btn_, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(cancel_btn_, cancel_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *cancel_label = lv_label_create(cancel_btn_);
  lv_obj_center(cancel_label);
  lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(cancel_label,
                              lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(cancel_label, "Cancel");

  // --- Delete button ---
  delete_btn_ = lv_button_create(picker_panel_);
  lv_obj_set_size(delete_btn_, 150, 60);
  lv_obj_align(delete_btn_, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_set_style_bg_color(delete_btn_, lv_color_hex(theme::kColorAlarmFiring), 0);
  lv_obj_set_style_radius(delete_btn_, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(delete_btn_, delete_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *del_label = lv_label_create(delete_btn_);
  lv_obj_center(del_label);
  lv_obj_set_style_text_font(del_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(del_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(del_label, "Delete");

  // --- Delete confirmation sub-overlay ---
  build_delete_confirm(picker_panel_);

  // --- On-screen keyboard for the label textarea ---
  keyboard_ = lv_keyboard_create(picker_overlay_);
  lv_obj_set_size(keyboard_, theme::kScreenWidth - 40, theme::kScreenHeight / 2);
  lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard_, keyboard_ready_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard_, keyboard_ready_cb, LV_EVENT_CANCEL, nullptr);

  // Show keyboard when label textarea gains focus; hide on defocus.
  lv_obj_add_event_cb(label_input_, label_focus_cb, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(label_input_, label_defocus_cb, LV_EVENT_DEFOCUSED, nullptr);
}

// ---------------------------------------------------------------------------
// Show the time picker for a given alarm.
// ---------------------------------------------------------------------------
void ui_show_time_picker(uint8_t alarm_index, uint8_t hour, uint8_t minute,
                         uint8_t days_mask, const char *label,
                         bool time_format_24h) {
  if (!picker_overlay_) {
    return;
  }

  editing_index_ = alarm_index;
  picker_24h_mode_ = time_format_24h;

  // Configure rollers based on time format.
  if (time_format_24h) {
    lv_roller_set_options(hour_roller_, hour_opts_24h_, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(hour_roller_, hour, LV_ANIM_OFF);
    lv_roller_set_selected(minute_roller_, minute, LV_ANIM_OFF);
    lv_obj_add_flag(ampm_roller_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_roller_set_options(hour_roller_, hour_opts_12h_, LV_ROLLER_MODE_INFINITE);
    auto [h12, is_pm] = hour_24_to_12(hour);
    lv_roller_set_selected(hour_roller_, h12 - 1, LV_ANIM_OFF);
    lv_roller_set_selected(minute_roller_, minute, LV_ANIM_OFF);
    lv_roller_set_selected(ampm_roller_, is_pm ? 1 : 0, LV_ANIM_OFF);
    lv_obj_clear_flag(ampm_roller_, LV_OBJ_FLAG_HIDDEN);
  }

  // Set day buttons.
  for (uint8_t d = 0; d < 7; d++) {
    if (days_mask & (1 << d)) {
      lv_obj_add_state(day_btns_[d], LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(day_btns_[d], LV_STATE_CHECKED);
    }
  }

  // Set label text.
  lv_textarea_set_text(label_input_, (label != nullptr) ? label : "");

  // Hide delete confirmation if it was visible.
  if (delete_confirm_overlay_) {
    lv_obj_add_flag(delete_confirm_overlay_, LV_OBJ_FLAG_HIDDEN);
  }

  // Show overlay.
  lv_obj_clear_flag(picker_overlay_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(picker_overlay_);
}

// ---------------------------------------------------------------------------
// Hide the time picker.
// ---------------------------------------------------------------------------
void ui_hide_time_picker() {
  if (keyboard_) {
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  }
  if (picker_overlay_) {
    lv_obj_add_flag(picker_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace alarmclock

#endif  // UNIT_TEST
