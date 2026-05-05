// Settings page — volume, brightness, alarm sound selection.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_theme.h"
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
static lv_obj_t *sound_label_ = nullptr;

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

// ---------------------------------------------------------------------------
// Build the settings page.
// ---------------------------------------------------------------------------
void ui_build_settings_page(lv_obj_t *parent) {
  // Title.
  title_label_ = lv_label_create(parent);
  lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(title_label_, "Settings");

  // --- Volume section ---
  lv_obj_t *vol_title = lv_label_create(parent);
  lv_obj_align(vol_title, LV_ALIGN_TOP_LEFT, 30, 70);
  lv_obj_set_style_text_font(vol_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(vol_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(vol_title, "Volume");

  volume_slider_ = lv_slider_create(parent);
  lv_obj_set_width(volume_slider_, theme::kScreenWidth - 160);
  lv_obj_align(volume_slider_, LV_ALIGN_TOP_LEFT, 30, 105);
  lv_slider_set_range(volume_slider_, 0, 100);
  lv_slider_set_value(volume_slider_, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(theme::kColorPrimary), LV_PART_KNOB);
  lv_obj_add_event_cb(volume_slider_, volume_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  volume_label_ = lv_label_create(parent);
  lv_obj_align(volume_label_, LV_ALIGN_TOP_RIGHT, -30, 98);
  lv_obj_set_style_text_font(volume_label_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(volume_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(volume_label_, "50%");

  // --- Brightness section ---
  lv_obj_t *bright_title = lv_label_create(parent);
  lv_obj_align(bright_title, LV_ALIGN_TOP_LEFT, 30, 160);
  lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(bright_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(bright_title, "Brightness");

  brightness_slider_ = lv_slider_create(parent);
  lv_obj_set_width(brightness_slider_, theme::kScreenWidth - 160);
  lv_obj_align(brightness_slider_, LV_ALIGN_TOP_LEFT, 30, 195);
  lv_slider_set_range(brightness_slider_, 0, 100);
  lv_slider_set_value(brightness_slider_, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(theme::kColorPrimary), LV_PART_KNOB);
  lv_obj_add_event_cb(brightness_slider_, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  brightness_label_ = lv_label_create(parent);
  lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -30, 188);
  lv_obj_set_style_text_font(brightness_label_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(brightness_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(brightness_label_, "50%");

  // --- Alarm sound section ---
  lv_obj_t *sound_title = lv_label_create(parent);
  lv_obj_align(sound_title, LV_ALIGN_TOP_LEFT, 30, 260);
  lv_obj_set_style_text_font(sound_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(sound_title, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(sound_title, "Alarm Sound");

  // TODO: Replace with a dropdown or list of alarm sounds.
  sound_label_ = lv_label_create(parent);
  lv_obj_align(sound_label_, LV_ALIGN_TOP_LEFT, 30, 295);
  lv_obj_set_style_text_font(sound_label_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sound_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(sound_label_, "Classic Beep");

  // --- Snooze duration ---
  // TODO: Add snooze duration selector (5/9/10/15 min).

  // --- 12/24 hour format toggle ---
  // TODO: Add time format toggle.
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

}  // namespace alarmclock

#endif  // UNIT_TEST
