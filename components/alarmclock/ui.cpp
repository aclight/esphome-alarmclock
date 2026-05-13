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
static bool animating_ = false;

// Touch tracking for swipe detection.
static lv_coord_t touch_start_x_ = 0;
static lv_coord_t touch_start_y_ = 0;
static bool tracking_swipe_ = false;

// Maximum vertical displacement to still count as a horizontal swipe.
static constexpr lv_coord_t kSwipeMaxVertical = 40;

// ---------------------------------------------------------------------------
// Swipe gesture handler (attached to each page).
// ---------------------------------------------------------------------------
static void swipe_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    touch_start_x_ = point.x;
    touch_start_y_ = point.y;
    tracking_swipe_ = true;
  } else if (code == LV_EVENT_RELEASED && tracking_swipe_) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    lv_coord_t dx = point.x - touch_start_x_;
    lv_coord_t dy = point.y - touch_start_y_;
    tracking_swipe_ = false;

    // Only trigger page switch for predominantly horizontal swipes.
    if (dy < -kSwipeMaxVertical || dy > kSwipeMaxVertical) {
      return;
    }

    if (dx < -theme::kSwipeThreshold) {
      ui_next_page();
    } else if (dx > theme::kSwipeThreshold) {
      ui_prev_page();
    }
  }
}

// ---------------------------------------------------------------------------
// Page transition animation helpers.
// ---------------------------------------------------------------------------
static void anim_x_cb(void *obj, int32_t x) {
  lv_obj_set_x(static_cast<lv_obj_t *>(obj), static_cast<lv_coord_t>(x));
}

static void anim_old_page_done_cb(lv_anim_t *a) {
  lv_obj_t *obj = static_cast<lv_obj_t *>(a->var);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  // Reset position so it's ready for next transition.
  lv_obj_set_x(obj, 0);
}

static void anim_new_page_done_cb(lv_anim_t * /*a*/) {
  animating_ = false;
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

  // Enable scrolling on the settings page so all controls are reachable.
  lv_obj_add_flag(pages_[theme::kPageSettings], LV_OBJ_FLAG_SCROLLABLE);

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

void ui_show_page(uint8_t page_index) {
  if (page_index >= theme::kPageCount) {
    return;
  }
  if (page_index == current_page_) {
    return;
  }
  if (animating_) {
    return;  // Don't interrupt ongoing animation.
  }

  lv_obj_t *old_page = pages_[current_page_];
  lv_obj_t *new_page = pages_[page_index];

  // Determine slide direction: positive index = slide left, negative = slide right.
  bool slide_left = (page_index > current_page_);
  // Handle wraparound: going from last to first page slides left,
  // going from first to last page slides right.
  if (current_page_ == theme::kPageCount - 1 && page_index == 0) {
    slide_left = true;
  } else if (current_page_ == 0 && page_index == theme::kPageCount - 1) {
    slide_left = false;
  }

  int16_t screen_w = theme::kScreenWidth;
  int16_t old_end = slide_left ? -screen_w : screen_w;
  int16_t new_start = slide_left ? screen_w : -screen_w;

  current_page_ = page_index;
  animating_ = true;

  // Update page indicator dots.
  ui_update_page_dots(page_index);

  // Position new page at the off-screen start and make visible.
  lv_obj_set_x(new_page, new_start);
  lv_obj_clear_flag(new_page, LV_OBJ_FLAG_HIDDEN);

  // Animate old page out.
  lv_anim_t anim_old;
  lv_anim_init(&anim_old);
  lv_anim_set_var(&anim_old, old_page);
  lv_anim_set_values(&anim_old, 0, old_end);
  lv_anim_set_time(&anim_old, theme::kPageAnimDuration);
  lv_anim_set_path_cb(&anim_old, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim_old, anim_x_cb);
  lv_anim_set_ready_cb(&anim_old, anim_old_page_done_cb);
  lv_anim_start(&anim_old);

  // Animate new page in.
  lv_anim_t anim_new;
  lv_anim_init(&anim_new);
  lv_anim_set_var(&anim_new, new_page);
  lv_anim_set_values(&anim_new, new_start, 0);
  lv_anim_set_time(&anim_new, theme::kPageAnimDuration);
  lv_anim_set_path_cb(&anim_new, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim_new, anim_x_cb);
  lv_anim_set_ready_cb(&anim_new, anim_new_page_done_cb);
  lv_anim_start(&anim_new);
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
