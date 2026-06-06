// Settings page — volume, brightness, alarm sound, snooze duration, time format.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_theme.h"
#include "alarmclock.h"
#include "lvgl.h"
#include <cstdio>

namespace alarmclock {

// Forward declaration.
const UiCallbacks &ui_get_callbacks();

// ---------------------------------------------------------------------------
// Static widgets.
// ---------------------------------------------------------------------------
static lv_obj_t *title_label_ = nullptr;
static lv_obj_t *volume_slider_ = nullptr;
static lv_obj_t *volume_label_ = nullptr;
static lv_obj_t *brightness_slider_ = nullptr;
static lv_obj_t *brightness_label_ = nullptr;
static lv_obj_t *sound_roller_ = nullptr;
static lv_obj_t *snooze_btns_[kSnoozeDurationOptionCount] = {};
static lv_obj_t *pre_alarm_btns_[kPreAlarmOptionCount] = {};
static lv_obj_t *time_format_switch_ = nullptr;
static lv_obj_t *time_format_label_ = nullptr;

// Track currently selected indices for button-row styling.
static uint8_t snooze_selected_ = 1;
static uint8_t pre_alarm_selected_ = 1;

// Snooze button labels.
static const char *kSnoozeLabels[kSnoozeDurationOptionCount] = {
    "5 min", "9 min", "10 min", "15 min"};

// Pre-alarm button labels.
static const char *kPreAlarmLabels[kPreAlarmOptionCount] = {
    "Off", "5 min", "10 min", "15 min"};

// ---------------------------------------------------------------------------
// Helpers for button-row selection styling.
// ---------------------------------------------------------------------------
static void style_option_btn(lv_obj_t *btn, bool selected) {
  if (selected) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(theme::kColorAccent), 0);
  } else {
    lv_obj_set_style_bg_color(btn, lv_color_hex(theme::kColorMuted), 0);
  }
}

static void update_snooze_btn_styles() {
  for (uint8_t i = 0; i < kSnoozeDurationOptionCount; ++i) {
    if (snooze_btns_[i]) {
      style_option_btn(snooze_btns_[i], i == snooze_selected_);
    }
  }
}

static void update_pre_alarm_btn_styles() {
  for (uint8_t i = 0; i < kPreAlarmOptionCount; ++i) {
    if (pre_alarm_btns_[i]) {
      style_option_btn(pre_alarm_btns_[i], i == pre_alarm_selected_);
    }
  }
}

// ---------------------------------------------------------------------------
// Event handlers.
// ---------------------------------------------------------------------------
static void home_btn_cb(lv_event_t *e) {
  (void)e;
  ui_show_clock_page();
}

static void volume_slider_cb(lv_event_t *e) {
  lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
  int32_t val = lv_slider_get_value(slider);
  float volume = static_cast<float>(val) / 100.0f;

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", val);
  if (volume_label_) {
    lv_label_set_text(volume_label_, buf);
  }

  const auto &cb = ui_get_callbacks();
  if (cb.on_volume_change) {
    cb.on_volume_change(volume);
  }
}

static void brightness_slider_cb(lv_event_t *e) {
  lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
  int32_t val = lv_slider_get_value(slider);
  float brightness = static_cast<float>(val) / 100.0f;

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", val);
  if (brightness_label_) {
    lv_label_set_text(brightness_label_, buf);
  }

  const auto &cb = ui_get_callbacks();
  if (cb.on_brightness_change) {
    cb.on_brightness_change(brightness);
  }
}

static void sound_dropdown_cb(lv_event_t *e) {
  lv_obj_t *roller = static_cast<lv_obj_t *>(lv_event_get_target(e));
  uint32_t sel = lv_roller_get_selected(roller);
  const auto &cb = ui_get_callbacks();
  if (cb.on_sound_change) {
    cb.on_sound_change(static_cast<uint8_t>(sel));
  }
  // Auto-preview the selected sound.
  if (cb.on_sound_preview) {
    cb.on_sound_preview(static_cast<uint8_t>(sel));
  }
}

static void snooze_btn_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  snooze_selected_ = index;
  update_snooze_btn_styles();
  const auto &cb = ui_get_callbacks();
  if (cb.on_snooze_duration_change) {
    cb.on_snooze_duration_change(index);
  }
}

static void time_format_switch_cb(lv_event_t *e) {
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  bool is_24h = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (time_format_label_) {
    lv_label_set_text(time_format_label_, is_24h ? "24h" : "12h");
  }
  const auto &cb = ui_get_callbacks();
  if (cb.on_time_format_change) {
    cb.on_time_format_change(is_24h);
  }
}

static void pre_alarm_btn_cb(lv_event_t *e) {
  uint8_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  pre_alarm_selected_ = index;
  update_pre_alarm_btn_styles();
  const auto &cb = ui_get_callbacks();
  if (cb.on_pre_alarm_change) {
    cb.on_pre_alarm_change(index);
  }
}

// ---------------------------------------------------------------------------
// Helper: create a transparent, borderless, non-scrollable row container.
// ---------------------------------------------------------------------------
static lv_obj_t *create_row(lv_obj_t *parent, int16_t width, int16_t height) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, width, height);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  return row;
}

// ---------------------------------------------------------------------------
// Build the settings page.
// ---------------------------------------------------------------------------
void ui_build_settings_page(lv_obj_t *parent) {
  // Use flex column layout so widgets flow automatically.
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(parent, 10, 0);
  lv_obj_set_style_pad_bottom(parent, 30, 0);
  lv_obj_set_style_pad_row(parent, 10, 0);
  // Enable vertical scrolling.
  lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  // Avoid elastic bounce/momentum animations; they add redraw cost and make
  // the page feel sluggish on this hardware.
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_time(parent, 0, LV_PART_MAIN);

  // --- Title row with home button ---
  lv_obj_t *title_row = create_row(parent, theme::kScreenWidth - 40, 64);
  lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  title_label_ = lv_label_create(title_row);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(title_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(title_label_, "Settings");

  lv_obj_t *home_btn = lv_button_create(title_row);
  lv_obj_set_size(home_btn, theme::kNavButtonWidth, theme::kNavButtonHeight);
  lv_obj_set_style_bg_color(home_btn, lv_color_hex(theme::kColorAccent), 0);
  lv_obj_set_style_radius(home_btn, theme::kButtonRadius, 0);
  lv_obj_add_event_cb(home_btn, home_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *home_label = lv_label_create(home_btn);
  lv_obj_center(home_label);
  lv_obj_set_style_text_font(home_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(home_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(home_label, "Home");

  // --- Volume section ---
  lv_obj_t *vol_title = lv_label_create(parent);
  lv_obj_set_style_text_font(vol_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(vol_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(vol_title, "Volume");
  lv_obj_set_width(vol_title, theme::kScreenWidth - 60);

  lv_obj_t *vol_row = create_row(parent, theme::kScreenWidth - 60, 56);
  lv_obj_set_flex_align(vol_row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(vol_row, 16, 0);

  volume_slider_ = lv_slider_create(vol_row);
  lv_obj_set_width(volume_slider_, theme::kScreenWidth - 300);
  lv_obj_set_height(volume_slider_, 14);
  lv_slider_set_range(volume_slider_, 0, 100);
  lv_slider_set_value(volume_slider_, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorPrimary), LV_PART_KNOB);
  lv_obj_set_style_outline_width(volume_slider_, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(volume_slider_, volume_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_set_style_pad_all(volume_slider_, 8, LV_PART_KNOB);

  volume_label_ = lv_label_create(vol_row);
  lv_obj_set_style_text_font(volume_label_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(volume_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(volume_label_, "50%");
  lv_obj_set_style_min_width(volume_label_, 80, 0);

  // --- Brightness section ---
  lv_obj_t *bright_title = lv_label_create(parent);
  lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(bright_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(bright_title, "Brightness");
  lv_obj_set_width(bright_title, theme::kScreenWidth - 60);

  lv_obj_t *bright_row = create_row(parent, theme::kScreenWidth - 60, 56);
  lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bright_row, 16, 0);

  brightness_slider_ = lv_slider_create(bright_row);
  lv_obj_set_width(brightness_slider_, theme::kScreenWidth - 300);
  lv_obj_set_height(brightness_slider_, 14);
  lv_slider_set_range(brightness_slider_, 0, 100);
  lv_slider_set_value(brightness_slider_, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorPrimary), LV_PART_KNOB);
  lv_obj_set_style_outline_width(brightness_slider_, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(brightness_slider_, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_set_style_pad_all(brightness_slider_, 8, LV_PART_KNOB);

  brightness_label_ = lv_label_create(bright_row);
  lv_obj_set_style_text_font(brightness_label_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(brightness_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(brightness_label_, "50%");
  lv_obj_set_style_min_width(brightness_label_, 80, 0);

  // --- Alarm sound section ---
  lv_obj_t *sound_title = lv_label_create(parent);
  lv_obj_set_style_text_font(sound_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(sound_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(sound_title, "Alarm Sound");
  lv_obj_set_width(sound_title, theme::kScreenWidth - 60);

  // Build roller options string from kAlarmSounds.
  static char sound_options[128];
  size_t offset = 0;
  for (uint8_t i = 0; i < kAlarmSoundCount; ++i) {
    if (i > 0 && offset < sizeof(sound_options) - 1) {
      sound_options[offset++] = '\n';
    }
    size_t name_len = strlen(kAlarmSounds[i].name);
    if (offset + name_len < sizeof(sound_options) - 1) {
      memcpy(&sound_options[offset], kAlarmSounds[i].name, name_len);
      offset += name_len;
    }
  }
  sound_options[offset] = '\0';

  sound_roller_ = lv_roller_create(parent);
  lv_roller_set_options(sound_roller_, sound_options, LV_ROLLER_MODE_NORMAL);
  lv_obj_set_width(sound_roller_, theme::kScreenWidth - 220);
  lv_obj_set_style_text_font(sound_roller_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_outline_width(sound_roller_, 0, LV_PART_MAIN);
  lv_roller_set_visible_row_count(sound_roller_, 2);
  lv_obj_add_event_cb(sound_roller_, sound_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // --- Snooze duration section (4 buttons in a row) ---
  lv_obj_t *snooze_title = lv_label_create(parent);
  lv_obj_set_style_text_font(snooze_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(snooze_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(snooze_title, "Snooze Duration");
  lv_obj_set_width(snooze_title, theme::kScreenWidth - 60);

  lv_obj_t *snooze_row = create_row(parent, theme::kScreenWidth - 60, 60);
  lv_obj_set_style_pad_column(snooze_row, 10, 0);
  lv_obj_set_flex_align(snooze_row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  for (uint8_t i = 0; i < kSnoozeDurationOptionCount; ++i) {
    snooze_btns_[i] = lv_button_create(snooze_row);
    lv_obj_set_size(snooze_btns_[i], 160, 56);
    lv_obj_set_style_radius(snooze_btns_[i], 8, 0);
    lv_obj_add_event_cb(snooze_btns_[i], snooze_btn_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    lv_obj_t *label = lv_label_create(snooze_btns_[i]);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_text(label, kSnoozeLabels[i]);
  }
  update_snooze_btn_styles();

  // --- Pre-alarm notification section (4 buttons in a row) ---
  lv_obj_t *pre_alarm_title = lv_label_create(parent);
  lv_obj_set_style_text_font(pre_alarm_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(pre_alarm_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(pre_alarm_title, "Pre-alarm Alert");
  lv_obj_set_width(pre_alarm_title, theme::kScreenWidth - 60);

  lv_obj_t *pre_alarm_row = create_row(parent, theme::kScreenWidth - 60, 60);
  lv_obj_set_style_pad_column(pre_alarm_row, 10, 0);
  lv_obj_set_flex_align(pre_alarm_row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  for (uint8_t i = 0; i < kPreAlarmOptionCount; ++i) {
    pre_alarm_btns_[i] = lv_button_create(pre_alarm_row);
    lv_obj_set_size(pre_alarm_btns_[i], 160, 56);
    lv_obj_set_style_radius(pre_alarm_btns_[i], 8, 0);
    lv_obj_add_event_cb(pre_alarm_btns_[i], pre_alarm_btn_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    lv_obj_t *label = lv_label_create(pre_alarm_btns_[i]);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_text(label, kPreAlarmLabels[i]);
  }

  // --- 12/24 hour format toggle ---
  lv_obj_t *format_row = create_row(parent, theme::kScreenWidth - 60, 56);
  lv_obj_set_flex_align(format_row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(format_row, 15, 0);

  lv_obj_t *format_title = lv_label_create(format_row);
  lv_obj_set_style_text_font(format_title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(format_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(format_title, "Time Format");

  time_format_switch_ = lv_switch_create(format_row);
  lv_obj_set_size(time_format_switch_, 70, 40);
  lv_obj_set_style_bg_color(time_format_switch_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(time_format_switch_, lv_color_hex(theme::kColorAccent),
                            static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
  lv_obj_add_event_cb(time_format_switch_, time_format_switch_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  time_format_label_ = lv_label_create(format_row);
  lv_obj_set_style_text_font(time_format_label_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(time_format_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(time_format_label_, "12h");

  update_pre_alarm_btn_styles();
}

// ---------------------------------------------------------------------------
// Public update functions.
// ---------------------------------------------------------------------------
void ui_update_volume(float volume) {
  if (volume_slider_) {
    lv_slider_set_value(volume_slider_, static_cast<int32_t>(volume * 100), LV_ANIM_OFF);
  }
  if (volume_label_) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(volume * 100));
    lv_label_set_text(volume_label_, buf);
  }
}

void ui_update_brightness(float brightness) {
  if (brightness_slider_) {
    lv_slider_set_value(brightness_slider_, static_cast<int32_t>(brightness * 100), LV_ANIM_OFF);
  }
  if (brightness_label_) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(brightness * 100));
    lv_label_set_text(brightness_label_, buf);
  }
}

void ui_update_sound_selection(uint8_t sound_index) {
  if (sound_roller_) {
    if (sound_index >= kAlarmSoundCount) {
      sound_index = 0;
    }
    lv_roller_set_selected(sound_roller_, sound_index, LV_ANIM_OFF);
  }
}

void ui_update_snooze_selection(uint8_t option_index) {
  if (option_index >= kSnoozeDurationOptionCount) {
    option_index = 1;  // default: 9 min
  }
  snooze_selected_ = option_index;
  update_snooze_btn_styles();
}

void ui_update_time_format(bool time_format_24h) {
  if (time_format_switch_) {
    if (time_format_24h) {
      lv_obj_add_state(time_format_switch_, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(time_format_switch_, LV_STATE_CHECKED);
    }
  }
  if (time_format_label_) {
    lv_label_set_text(time_format_label_, time_format_24h ? "24h" : "12h");
  }
}

void ui_update_pre_alarm_selection(uint8_t option_index) {
  if (option_index >= kPreAlarmOptionCount) {
    option_index = 1;  // default: 5 min
  }
  pre_alarm_selected_ = option_index;
  update_pre_alarm_btn_styles();
}

}  // namespace alarmclock

#endif  // UNIT_TEST
