// UI manager implementation — page container, swipe detection, overlay.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_theme.h"
#include "lvgl.h"

namespace alarmclock {

// ---------------------------------------------------------------------------
// Static state.
// ---------------------------------------------------------------------------
static lv_obj_t *pages_[theme::kPageCount] = {};
static lv_obj_t *firing_overlay_ = nullptr;
static uint8_t current_page_ = theme::kPageClock;
static UiCallbacks callbacks_ = {};

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

void ui_set_callbacks(const UiCallbacks &cb) {
  callbacks_ = cb;
}

void ui_init() {
  // Get the active screen (already created by ESPHome's LVGL component).
  lv_obj_t *scr = lv_scr_act();

  // Remove any objects ESPHome's LVGL component created (e.g. boot_page)
  // so they don't cover our C++ UI with their default theme background.
  lv_obj_clean(scr);

  lv_obj_set_style_bg_color(scr, lv_color_hex(theme::kColorBackground), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Create page containers (full-screen panels, only one visible at a time).
  for (uint8_t i = 0; i < theme::kPageCount; i++) {
    pages_[i] = lv_obj_create(scr);
    lv_obj_set_size(pages_[i], theme::kScreenWidth, theme::kScreenHeight);
    lv_obj_align(pages_[i], LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(pages_[i], lv_color_hex(theme::kColorBackground), 0);
    lv_obj_set_style_border_width(pages_[i], 0, 0);
    lv_obj_set_style_radius(pages_[i], 0, 0);
    lv_obj_set_style_pad_all(pages_[i], 0, 0);
    lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_SCROLLABLE);

    // Hide all pages initially.
    lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Build each page's content.
  ui_build_clock_page(pages_[theme::kPageClock]);
  ui_build_alarm_page(pages_[theme::kPageAlarms]);
  ui_build_settings_page(pages_[theme::kPageSettings]);

  // Create firing overlay (hidden by default, shown above everything).
  firing_overlay_ = lv_obj_create(scr);
  lv_obj_set_size(firing_overlay_, theme::kScreenWidth, theme::kScreenHeight);
  lv_obj_align(firing_overlay_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(firing_overlay_, lv_color_hex(theme::kColorBackground), 0);
  lv_obj_set_style_border_width(firing_overlay_, 0, 0);
  lv_obj_set_style_radius(firing_overlay_, 0, 0);
  lv_obj_clear_flag(firing_overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(firing_overlay_, LV_OBJ_FLAG_HIDDEN);
  ui_build_firing_overlay(firing_overlay_);

  // Create time picker overlay (hidden by default, shown above pages).
  ui_build_time_picker(scr);

  // Show the clock page (no animation on first show).
  current_page_ = theme::kPageClock;
  lv_obj_clear_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
}

void ui_show_clock_page() {
  ui_show_page(theme::kPageClock);
}

void ui_show_alarm_page() {
  ui_show_page(theme::kPageAlarms);
}

void ui_show_settings_page() {
  ui_show_page(theme::kPageSettings);
}

void ui_show_page(uint8_t page_index) {
  if (page_index >= theme::kPageCount) {
    return;
  }
  if (page_index == current_page_) {
    return;
  }

  lv_obj_t *old_page = pages_[current_page_];
  lv_obj_t *new_page = pages_[page_index];

  current_page_ = page_index;

  // Instant page switch — hide old, show new.
  lv_obj_add_flag(old_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_x(new_page, 0);
  lv_obj_clear_flag(new_page, LV_OBJ_FLAG_HIDDEN);
}

void ui_next_page() {
  ui_show_alarm_page();
}

void ui_prev_page() {
  ui_show_clock_page();
}

uint8_t ui_current_page() {
  return current_page_;
}

void ui_show_firing_overlay() {
  if (firing_overlay_) {
    lv_obj_clear_flag(firing_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(firing_overlay_);
  }
}

void ui_hide_firing_overlay() {
  if (firing_overlay_) {
    lv_obj_add_flag(firing_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

bool ui_is_firing_overlay_visible() {
  if (!firing_overlay_) {
    return false;
  }
  return !lv_obj_has_flag(firing_overlay_, LV_OBJ_FLAG_HIDDEN);
}

// Accessors for callbacks (used by page implementations).
const UiCallbacks &ui_get_callbacks() {
  return callbacks_;
}

}  // namespace alarmclock

#endif  // UNIT_TEST
