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

// Touch tracking for swipe detection.
static lv_coord_t touch_start_x_ = 0;
static bool tracking_swipe_ = false;

// ---------------------------------------------------------------------------
// Swipe gesture handler (attached to each page).
// ---------------------------------------------------------------------------
static void swipe_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    touch_start_x_ = point.x;
    tracking_swipe_ = true;
  } else if (code == LV_EVENT_RELEASED && tracking_swipe_) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    lv_coord_t dx = point.x - touch_start_x_;
    tracking_swipe_ = false;

    if (dx < -theme::kSwipeThreshold) {
      ui_next_page();
    } else if (dx > theme::kSwipeThreshold) {
      ui_prev_page();
    }
  }
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

void ui_set_callbacks(const UiCallbacks &cb) {
  callbacks_ = cb;
}

void ui_init() {
  // Get the active screen (already created by ESPHome's LVGL component).
  lv_obj_t *scr = lv_scr_act();
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

    // Attach swipe detection to each page.
    lv_obj_add_event_cb(pages_[i], swipe_event_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(pages_[i], swipe_event_cb, LV_EVENT_RELEASED, nullptr);

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

  // Show the clock page.
  ui_show_page(theme::kPageClock);
}

void ui_show_page(uint8_t page_index) {
  if (page_index >= theme::kPageCount) {
    return;
  }
  // Hide current page.
  if (current_page_ < theme::kPageCount && pages_[current_page_]) {
    lv_obj_add_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
  }
  // Show new page.
  current_page_ = page_index;
  lv_obj_clear_flag(pages_[current_page_], LV_OBJ_FLAG_HIDDEN);
}

void ui_next_page() {
  uint8_t next = (current_page_ + 1) % theme::kPageCount;
  ui_show_page(next);
}

void ui_prev_page() {
  uint8_t prev = (current_page_ == 0) ? theme::kPageCount - 1 : current_page_ - 1;
  ui_show_page(prev);
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
