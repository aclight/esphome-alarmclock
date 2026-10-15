// Configurable Home Assistant controls page.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "lvgl.h"

namespace alarmclock {

const UiCallbacks &ui_get_callbacks();

struct ControlUiState {
  UiControlType type = UiControlType::kToggle;
  std::string name;
  lv_obj_t *card = nullptr;
  lv_obj_t *state_label = nullptr;
  lv_obj_t *slider = nullptr;
  lv_obj_t *value_label = nullptr;
  bool available = false;
};

static ControlUiState controls_[theme::kMaxControls];
static uint8_t control_count_ = 0;
static lv_obj_t *empty_label_ = nullptr;

static void set_card_color(ControlUiState &control, bool active) {
  if (control.card == nullptr) {
    return;
  }
  uint32_t color = theme::kColorMuted;
  if (!control.available) {
    color = 0x181818;
  } else if (active) {
    color = theme::kColorAccent;
  }
  lv_obj_set_style_bg_color(control.card, lv_color_hex(color), 0);
}

static void control_clicked_cb(lv_event_t *event) {
  uint8_t index =
      static_cast<uint8_t>(reinterpret_cast<uintptr_t>(
          lv_event_get_user_data(event)));
  if (index >= control_count_) {
    return;
  }
  auto &control = controls_[index];
  if (control.type != UiControlType::kAction && !control.available) {
    return;
  }
  const auto &callbacks = ui_get_callbacks();
  if (callbacks.on_control_press != nullptr) {
    callbacks.on_control_press(index);
  }
}

static void dimmer_released_cb(lv_event_t *event) {
  uint8_t index =
      static_cast<uint8_t>(reinterpret_cast<uintptr_t>(
          lv_event_get_user_data(event)));
  if (index >= control_count_ || !controls_[index].available) {
    return;
  }
  lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(event));
  const float brightness =
      static_cast<float>(lv_slider_get_value(slider)) / 100.0f;
  const auto &callbacks = ui_get_callbacks();
  if (callbacks.on_control_brightness_change != nullptr) {
    callbacks.on_control_brightness_change(index, brightness);
  }
}

void ui_add_control(uint8_t index, UiControlType type, const char *name) {
  if (index >= theme::kMaxControls) {
    return;
  }
  control_count_ = std::max<uint8_t>(control_count_, index + 1);
  controls_[index].type = type;
  controls_[index].name = name == nullptr ? "" : name;
  controls_[index].available = type == UiControlType::kAction;
}

void ui_build_controls_page(lv_obj_t *parent) {
  ui_create_page_header(parent, "Controls");

  if (control_count_ == 0) {
    empty_label_ = lv_label_create(parent);
    lv_obj_align(empty_label_, LV_ALIGN_CENTER, 0, 35);
    lv_obj_set_style_text_font(empty_label_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(
        empty_label_, lv_color_hex(theme::kColorSecondary), 0);
    lv_label_set_text(empty_label_, "No controls configured");
    return;
  }

  static constexpr int16_t kCardWidth = 240;
  static constexpr int16_t kCardHeight = 170;
  static constexpr int16_t kGapX = 20;
  static constexpr int16_t kGapY = 15;
  static constexpr int16_t kStartX = 20;
  static constexpr int16_t kStartY = 95;

  for (uint8_t i = 0; i < control_count_; ++i) {
    auto &control = controls_[i];
    const int16_t column = i % 3;
    const int16_t row = i / 3;
    control.card = lv_button_create(parent);
    lv_obj_set_pos(control.card,
                   kStartX + column * (kCardWidth + kGapX),
                   kStartY + row * (kCardHeight + kGapY));
    lv_obj_set_size(control.card, kCardWidth, kCardHeight);
    lv_obj_set_style_radius(control.card, theme::kButtonRadius, 0);
    lv_obj_set_style_border_width(control.card, 0, 0);
    lv_obj_set_style_pad_all(control.card, 12, 0);
    lv_obj_clear_flag(control.card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(
        control.card, control_clicked_cb, LV_EVENT_CLICKED,
        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

    lv_obj_t *name_label = lv_label_create(control.card);
    lv_obj_set_width(name_label, kCardWidth - 24);
    lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(
        name_label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(name_label, control.name.c_str());

    control.state_label = lv_label_create(control.card);
    lv_obj_align(control.state_label, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_style_text_font(
        control.state_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(
        control.state_label, lv_color_hex(theme::kColorPrimary), 0);

    if (control.type == UiControlType::kDimmer) {
      control.slider = lv_slider_create(control.card);
      lv_obj_set_size(control.slider, kCardWidth - 55, 28);
      lv_obj_align(control.slider, LV_ALIGN_BOTTOM_LEFT, 0, -5);
      lv_slider_set_range(control.slider, 0, 100);
      lv_obj_add_event_cb(
          control.slider, dimmer_released_cb, LV_EVENT_RELEASED,
          reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

      control.value_label = lv_label_create(control.card);
      lv_obj_align(control.value_label, LV_ALIGN_BOTTOM_RIGHT, 0, -6);
      lv_obj_set_style_text_font(
          control.value_label, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(
          control.value_label, lv_color_hex(theme::kColorPrimary), 0);
      lv_label_set_text(control.value_label, "--");
    }

    if (control.type == UiControlType::kAction) {
      lv_label_set_text(control.state_label, "Run");
    } else {
      lv_label_set_text(control.state_label, "Unavailable");
    }
    set_card_color(control, false);
  }
}

void ui_update_control_state(uint8_t index, const char *state,
                             bool available) {
  if (index >= control_count_) {
    return;
  }
  auto &control = controls_[index];
  control.available = available;
  if (control.state_label == nullptr) {
    return;
  }
  if (!available) {
    lv_label_set_text(control.state_label, "Unavailable");
    set_card_color(control, false);
    return;
  }
  const bool active = state != nullptr && strcmp(state, "on") == 0;
  lv_label_set_text(control.state_label, active ? "On" : "Off");
  set_card_color(control, active);
}

void ui_update_control_brightness(uint8_t index, float brightness,
                                  bool available) {
  if (index >= control_count_) {
    return;
  }
  auto &control = controls_[index];
  if (control.slider == nullptr || control.value_label == nullptr) {
    return;
  }
  control.available = available;
  if (!available || !std::isfinite(brightness)) {
    lv_obj_add_state(control.slider, LV_STATE_DISABLED);
    lv_label_set_text(control.value_label, "--");
    return;
  }
  lv_obj_clear_state(control.slider, LV_STATE_DISABLED);
  const int value = static_cast<int>(
      std::round(std::max(0.0f, std::min(1.0f, brightness)) * 100.0f));
  lv_slider_set_value(control.slider, value, LV_ANIM_OFF);
  char text[8];
  snprintf(text, sizeof(text), "%d%%", value);
  lv_label_set_text(control.value_label, text);
}

}  // namespace alarmclock

#endif  // UNIT_TEST
