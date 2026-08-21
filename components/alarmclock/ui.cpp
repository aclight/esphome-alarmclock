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
static lv_color_filter_dsc_t content_dim_filter_;
static uint8_t content_dim_opacity_ = 0;

static lv_color_t content_dim_filter_cb(
    const lv_color_filter_dsc_t *filter, lv_color_t color, lv_opa_t opacity) {
  (void)filter;
  return lv_color_mix(lv_color_black(), color, opacity);
}

static void home_button_cb(lv_event_t *event) {
  (void)event;
  ui_show_clock_page();
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

void ui_set_callbacks(const UiCallbacks &cb) {
  callbacks_ = cb;
}

void ui_create_page_header(lv_obj_t *parent, const char *title) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, theme::kScreenWidth, theme::kPageHeaderHeight);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(header, lv_color_hex(theme::kColorBackground), 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_pad_all(header, 10, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_label = lv_label_create(header);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(
    title_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(title_label, title);

  lv_obj_t *home_button = lv_button_create(header);
  lv_obj_set_size(
    home_button, theme::kNavButtonWidth, theme::kNavButtonHeight);
  lv_obj_set_style_bg_color(
    home_button, lv_color_hex(theme::kColorAccent), 0);
  lv_obj_set_style_border_width(home_button, 0, 0);
  lv_obj_set_style_radius(home_button, theme::kButtonRadius, 0);
  lv_obj_clear_flag(home_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_add_event_cb(
    home_button, home_button_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *home_label = lv_label_create(home_button);
  lv_obj_center(home_label);
  lv_obj_set_style_text_font(home_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(
    home_label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(home_label, LV_SYMBOL_HOME);
}

void ui_init() {
  // Get the active screen (already created by ESPHome's LVGL component).
  lv_obj_t *scr = lv_scr_act();

  // Remove any objects ESPHome's LVGL component created (e.g. boot_page)
  // so they don't cover our C++ UI with their default theme background.
  lv_obj_clean(scr);

  lv_obj_set_style_bg_color(scr, lv_color_hex(theme::kColorBackground), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_color_filter_dsc_init(&content_dim_filter_, content_dim_filter_cb);
  lv_obj_set_style_color_filter_dsc(scr, &content_dim_filter_, 0);
  lv_obj_set_style_color_filter_opa(scr, content_dim_opacity_, 0);

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
  // Alarm controls must remain fully colored and legible at every setting.
  lv_obj_set_style_color_filter_opa(firing_overlay_, LV_OPA_TRANSP, 0);
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
  if (current_page_ == theme::kPageSettings &&
      callbacks_.on_sound_preview_cancel != nullptr) {
    callbacks_.on_sound_preview_cancel();
  }

  lv_obj_t *old_page = pages_[current_page_];
  lv_obj_t *new_page = pages_[page_index];

  current_page_ = page_index;

  // Instant page switch — hide old, show new.
  lv_obj_add_flag(old_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_x(new_page, 0);
  lv_obj_clear_flag(new_page, LV_OBJ_FLAG_HIDDEN);
}

uint8_t ui_current_page() {
  return current_page_;
}

void ui_set_content_brightness(float brightness) {
  const uint8_t opacity = compute_content_dim_opacity(brightness);
  if (opacity == content_dim_opacity_) {
    return;
  }

  content_dim_opacity_ = opacity;
  lv_obj_t *screen = lv_scr_act();
  if (screen != nullptr) {
    lv_obj_set_style_color_filter_opa(screen, opacity, 0);
  }
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
