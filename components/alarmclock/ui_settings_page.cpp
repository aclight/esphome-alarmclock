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
static lv_obj_t *snooze_roller_ = nullptr;
static lv_obj_t *time_format_switch_ = nullptr;
static lv_obj_t *time_format_label_ = nullptr;
static lv_obj_t *pre_alarm_roller_ = nullptr;

// ---------------------------------------------------------------------------
// Event handlers.
// ---------------------------------------------------------------------------
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
}

static void snooze_dropdown_cb(lv_event_t *e) {
  lv_obj_t *roller = static_cast<lv_obj_t *>(lv_event_get_target(e));
  uint32_t sel = lv_roller_get_selected(roller);
  const auto &cb = ui_get_callbacks();
  if (cb.on_snooze_duration_change) {
    cb.on_snooze_duration_change(static_cast<uint8_t>(sel));
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

static void pre_alarm_dropdown_cb(lv_event_t *e) {
  lv_obj_t *roller = static_cast<lv_obj_t *>(lv_event_get_target(e));
  uint32_t sel = lv_roller_get_selected(roller);
  const auto &cb = ui_get_callbacks();
  if (cb.on_pre_alarm_change) {
    cb.on_pre_alarm_change(static_cast<uint8_t>(sel));
  }
}

// ---------------------------------------------------------------------------
// Build the settings page.
// ---------------------------------------------------------------------------
void ui_build_settings_page(lv_obj_t *parent) {
  // Use flex column layout so widgets flow automatically without fragile Y values.
  lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(parent, 15, 0);
  lv_obj_set_style_pad_bottom(parent, 30, 0);
  lv_obj_set_style_pad_row(parent, 10, 0);

  // Title.
  title_label_ = lv_label_create(parent);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(title_label_, "Settings");

  // --- Volume section ---
  lv_obj_t *vol_title = lv_label_create(parent);
  lv_obj_set_style_text_font(vol_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(vol_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(vol_title, "Volume");
  lv_obj_set_width(vol_title, theme::kScreenWidth - 60);

  // Volume row (slider + label).
  lv_obj_t *vol_row = lv_obj_create(parent);
  lv_obj_set_size(vol_row, theme::kScreenWidth - 60, 40);
  lv_obj_set_flex_flow(vol_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(vol_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(vol_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(vol_row, 0, 0);
  lv_obj_set_style_pad_all(vol_row, 0, 0);
  lv_obj_clear_flag(vol_row, LV_OBJ_FLAG_SCROLLABLE);

  volume_slider_ = lv_slider_create(vol_row);
  lv_obj_set_flex_grow(volume_slider_, 1);
  lv_obj_set_height(volume_slider_, 10);
  lv_slider_set_range(volume_slider_, 0, 100);
  lv_slider_set_value(volume_slider_, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorPrimary), LV_PART_KNOB);
  lv_obj_add_event_cb(volume_slider_, volume_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_set_style_pad_all(volume_slider_, 8, LV_PART_KNOB);

  volume_label_ = lv_label_create(vol_row);
  lv_obj_set_style_text_font(volume_label_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(volume_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(volume_label_, "50%");
  lv_obj_set_style_min_width(volume_label_, 50, 0);

  // --- Brightness section ---
  lv_obj_t *bright_title = lv_label_create(parent);
  lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(bright_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(bright_title, "Brightness");
  lv_obj_set_width(bright_title, theme::kScreenWidth - 60);

  // Brightness row (slider + label).
  lv_obj_t *bright_row = lv_obj_create(parent);
  lv_obj_set_size(bright_row, theme::kScreenWidth - 60, 40);
  lv_obj_set_flex_flow(bright_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bright_row, 0, 0);
  lv_obj_set_style_pad_all(bright_row, 0, 0);
  lv_obj_clear_flag(bright_row, LV_OBJ_FLAG_SCROLLABLE);

  brightness_slider_ = lv_slider_create(bright_row);
  lv_obj_set_flex_grow(brightness_slider_, 1);
  lv_obj_set_height(brightness_slider_, 10);
  lv_slider_set_range(brightness_slider_, 0, 100);
  lv_slider_set_value(brightness_slider_, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorPrimary), LV_PART_KNOB);
  lv_obj_add_event_cb(brightness_slider_, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_set_style_pad_all(brightness_slider_, 8, LV_PART_KNOB);

  brightness_label_ = lv_label_create(bright_row);
  lv_obj_set_style_text_font(brightness_label_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(brightness_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(brightness_label_, "50%");
  lv_obj_set_style_min_width(brightness_label_, 50, 0);

  // --- Alarm sound section ---
  lv_obj_t *sound_title = lv_label_create(parent);
  lv_obj_set_style_text_font(sound_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(sound_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(sound_title, "Alarm Sound");
  lv_obj_set_width(sound_title, theme::kScreenWidth - 60);

  // Build dropdown options string from kAlarmSounds.
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
  lv_obj_set_width(sound_roller_, theme::kScreenWidth - 60);
  lv_obj_set_style_text_font(sound_roller_, &lv_font_montserrat_18, 0);
  lv_roller_set_visible_row_count(sound_roller_, 3);
  lv_obj_add_event_cb(sound_roller_, sound_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // --- Snooze duration section ---
  lv_obj_t *snooze_title = lv_label_create(parent);
  lv_obj_set_style_text_font(snooze_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(snooze_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(snooze_title, "Snooze Duration");
  lv_obj_set_width(snooze_title, theme::kScreenWidth - 60);

  snooze_roller_ = lv_roller_create(parent);
  lv_roller_set_options(snooze_roller_, "5 min\n9 min\n10 min\n15 min", LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(snooze_roller_, 1, LV_ANIM_OFF);  // default: 9 min
  lv_obj_set_width(snooze_roller_, theme::kScreenWidth - 60);
  lv_obj_set_style_text_font(snooze_roller_, &lv_font_montserrat_18, 0);
  lv_roller_set_visible_row_count(snooze_roller_, 2);
  lv_obj_add_event_cb(snooze_roller_, snooze_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // --- 12/24 hour format toggle ---
  lv_obj_t *format_row = lv_obj_create(parent);
  lv_obj_set_size(format_row, theme::kScreenWidth - 60, 40);
  lv_obj_set_flex_flow(format_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(format_row, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(format_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(format_row, 0, 0);
  lv_obj_set_style_pad_all(format_row, 0, 0);
  lv_obj_set_style_pad_column(format_row, 10, 0);
  lv_obj_clear_flag(format_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *format_title = lv_label_create(format_row);
  lv_obj_set_style_text_font(format_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(format_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(format_title, "Time Format");

  time_format_switch_ = lv_switch_create(format_row);
  lv_obj_set_style_bg_color(time_format_switch_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(time_format_switch_, lv_color_hex(theme::kColorAccent),
                            static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_CHECKED);
  lv_obj_add_event_cb(time_format_switch_, time_format_switch_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  time_format_label_ = lv_label_create(format_row);
  lv_obj_set_style_text_font(time_format_label_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(time_format_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(time_format_label_, "12h");

  // --- Pre-alarm notification section ---
  lv_obj_t *pre_alarm_title = lv_label_create(parent);
  lv_obj_set_style_text_font(pre_alarm_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(pre_alarm_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(pre_alarm_title, "Pre-alarm Alert");
  lv_obj_set_width(pre_alarm_title, theme::kScreenWidth - 60);

  pre_alarm_roller_ = lv_roller_create(parent);
  lv_roller_set_options(pre_alarm_roller_, "Off\n5 min\n10 min\n15 min", LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(pre_alarm_roller_, 1, LV_ANIM_OFF);  // default: 5 min
  lv_obj_set_width(pre_alarm_roller_, theme::kScreenWidth - 60);
  lv_obj_set_style_text_font(pre_alarm_roller_, &lv_font_montserrat_18, 0);
  lv_roller_set_visible_row_count(pre_alarm_roller_, 2);
  lv_obj_add_event_cb(pre_alarm_roller_, pre_alarm_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);
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
  if (snooze_roller_) {
    if (option_index >= kSnoozeDurationOptionCount) {
      option_index = 1;  // default: 9 min
    }
    lv_roller_set_selected(snooze_roller_, option_index, LV_ANIM_OFF);
  }
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
  if (pre_alarm_roller_) {
    if (option_index >= kPreAlarmOptionCount) {
      option_index = 1;  // default: 5 min
    }
    lv_roller_set_selected(pre_alarm_roller_, option_index, LV_ANIM_OFF);
  }
}

}  // namespace alarmclock

#endif  // UNIT_TEST
