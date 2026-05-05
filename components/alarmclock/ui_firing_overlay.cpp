// Firing overlay — shown when alarm fires. Big snooze/dismiss buttons.
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
static lv_obj_t *firing_time_label_ = nullptr;
static lv_obj_t *firing_message_label_ = nullptr;
static lv_obj_t *snooze_btn_ = nullptr;
static lv_obj_t *dismiss_btn_ = nullptr;

// Pulsing animation for the time label.
static lv_anim_t pulse_anim_;

// ---------------------------------------------------------------------------
// Animation callback — pulse opacity on the time label.
// ---------------------------------------------------------------------------
static void pulse_anim_cb(void *obj, int32_t val) {
  lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj),
                       static_cast<lv_opa_t>(val), 0);
}

// ---------------------------------------------------------------------------
// Event handlers.
// ---------------------------------------------------------------------------
static void snooze_btn_cb(lv_event_t *e) {
  (void)e;
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_snooze) {
    cb.on_alarm_snooze();
  }
}

static void dismiss_btn_cb(lv_event_t *e) {
  (void)e;
  const auto &cb = ui_get_callbacks();
  if (cb.on_alarm_dismiss) {
    cb.on_alarm_dismiss();
  }
}

// ---------------------------------------------------------------------------
// Build the firing overlay.
// ---------------------------------------------------------------------------
void ui_build_firing_overlay(lv_obj_t *parent) {
  // Current time (large, pulsing).
  firing_time_label_ = lv_label_create(parent);
  lv_obj_align(firing_time_label_, LV_ALIGN_CENTER, 0, theme::kFiringTimeY);
  lv_obj_set_style_text_font(firing_time_label_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(firing_time_label_,
                              lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(firing_time_label_, "7:00 AM");

  // "Alarm!" message.
  firing_message_label_ = lv_label_create(parent);
  lv_obj_align(firing_message_label_, LV_ALIGN_CENTER, 0,
               theme::kFiringTimeY - 60);
  lv_obj_set_style_text_font(firing_message_label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(firing_message_label_,
                              lv_color_hex(theme::kColorAlarmFiring), 0);
  lv_label_set_text(firing_message_label_, "ALARM");

  // Snooze button (large, easy to hit half-asleep).
  snooze_btn_ = lv_button_create(parent);
  lv_obj_align(snooze_btn_, LV_ALIGN_CENTER, 0, theme::kFiringSnoozeY);
  lv_obj_set_size(snooze_btn_, theme::kButtonWidth, theme::kButtonHeight);
  lv_obj_set_style_radius(snooze_btn_, theme::kButtonRadius, 0);
  lv_obj_set_style_bg_color(snooze_btn_, lv_color_hex(theme::kColorSnooze), 0);
  lv_obj_add_event_cb(snooze_btn_, snooze_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *snooze_label = lv_label_create(snooze_btn_);
  lv_obj_center(snooze_label);
  lv_obj_set_style_text_font(snooze_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(snooze_label, lv_color_hex(0x000000), 0);
  lv_label_set_text(snooze_label, "Snooze");

  // Dismiss button.
  dismiss_btn_ = lv_button_create(parent);
  lv_obj_align(dismiss_btn_, LV_ALIGN_CENTER, 0, theme::kFiringDismissY);
  lv_obj_set_size(dismiss_btn_, theme::kButtonWidth, theme::kButtonHeight);
  lv_obj_set_style_radius(dismiss_btn_, theme::kButtonRadius, 0);
  lv_obj_set_style_bg_color(dismiss_btn_, lv_color_hex(theme::kColorDismiss), 0);
  lv_obj_add_event_cb(dismiss_btn_, dismiss_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *dismiss_label = lv_label_create(dismiss_btn_);
  lv_obj_center(dismiss_label);
  lv_obj_set_style_text_font(dismiss_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(dismiss_label, lv_color_hex(0x000000), 0);
  lv_label_set_text(dismiss_label, "Dismiss");

  // Setup pulse animation on the time label (loops).
  lv_anim_init(&pulse_anim_);
  lv_anim_set_var(&pulse_anim_, firing_time_label_);
  lv_anim_set_values(&pulse_anim_, 255, 100);
  lv_anim_set_time(&pulse_anim_, 800);
  lv_anim_set_playback_time(&pulse_anim_, 800);
  lv_anim_set_repeat_count(&pulse_anim_, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&pulse_anim_, pulse_anim_cb);
  // Animation is started when overlay is shown.
}

// ---------------------------------------------------------------------------
// Public: update the firing overlay time display.
// ---------------------------------------------------------------------------
void ui_firing_update_time(uint8_t hour, uint8_t minute) {
  if (!firing_time_label_) {
    return;
  }
  const char *ampm = (hour >= 12) ? "PM" : "AM";
  uint8_t display_hour = hour % 12;
  if (display_hour == 0) {
    display_hour = 12;
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%d:%02d %s", display_hour, minute, ampm);
  lv_label_set_text(firing_time_label_, buf);
}

// ---------------------------------------------------------------------------
// Public: update the firing overlay label (e.g. "ALARM — Work").
// ---------------------------------------------------------------------------
void ui_firing_update_label(const char *label) {
  if (!firing_message_label_) {
    return;
  }
  if (label != nullptr && label[0] != '\0') {
    char buf[40];
    snprintf(buf, sizeof(buf), "ALARM \xE2\x80\x94 %s", label);
    lv_label_set_text(firing_message_label_, buf);
  } else {
    lv_label_set_text(firing_message_label_, "ALARM");
  }
}

// Start/stop the pulse animation.
void ui_firing_start_animation() {
  lv_anim_start(&pulse_anim_);
}

void ui_firing_stop_animation() {
  lv_anim_del(firing_time_label_, pulse_anim_cb);
  if (firing_time_label_) {
    lv_obj_set_style_opa(firing_time_label_, LV_OPA_COVER, 0);
  }
}

}  // namespace alarmclock

#endif  // UNIT_TEST
