// Configurable Home Assistant controls page.
#ifndef UNIT_TEST

#include "ui.h"
#include "ui_icons.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "lvgl.h"

namespace alarmclock {

const UiCallbacks &ui_get_callbacks();

static constexpr uint32_t kColorCardUnavailable = 0x181818;
static constexpr uint32_t kColorIconOn = 0xFFD54F;
static constexpr uint32_t kColorIconUnavailable = 0x555555;
static constexpr uint32_t kColorSwitchTrack = 0x1A1A1A;

struct ControlUiState {
  UiControlType type = UiControlType::kToggle;
  ControlIconKind icon_kind = ControlIconKind::kPower;
  std::string name;
  lv_obj_t *card = nullptr;
  lv_obj_t *icon_label = nullptr;
  lv_obj_t *state_label = nullptr;
  lv_obj_t *toggle_switch = nullptr;
  lv_obj_t *slider = nullptr;
  lv_obj_t *value_label = nullptr;
  bool available = false;
  bool active = false;
  float brightness = NAN;
};

static ControlUiState controls_[theme::kMaxControls];
static uint8_t control_count_ = 0;
static lv_obj_t *empty_label_ = nullptr;

static void render_control(ControlUiState &control) {
  if (control.card == nullptr) {
    return;
  }
  const bool is_action = control.type == UiControlType::kAction;
  const bool on = control.available && control.active && !is_action;

  uint32_t card_color = theme::kColorMuted;
  if (!control.available) {
    card_color = kColorCardUnavailable;
  } else if (on) {
    card_color = theme::kColorAccent;
  }
  lv_obj_set_style_bg_color(control.card, lv_color_hex(card_color), 0);

  uint32_t icon_color = theme::kColorSecondary;
  if (!control.available) {
    icon_color = kColorIconUnavailable;
  } else if (is_action) {
    icon_color = theme::kColorPrimary;
  } else if (on) {
    icon_color = control.icon_kind == ControlIconKind::kLight
                     ? kColorIconOn
                     : theme::kColorPrimary;
  }
  lv_label_set_text(control.icon_label,
                    control_icon(control.icon_kind, on));
  lv_obj_set_style_text_color(control.icon_label, lv_color_hex(icon_color),
                              0);

  if (control.state_label != nullptr) {
    lv_label_set_text(control.state_label,
                      !control.available ? "Unavailable"
                                         : (on ? "On" : "Off"));
  }

  if (control.toggle_switch != nullptr) {
    if (on) {
      lv_obj_add_state(control.toggle_switch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(control.toggle_switch, LV_STATE_CHECKED);
    }
    if (control.available) {
      lv_obj_clear_state(control.toggle_switch, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(control.toggle_switch, LV_STATE_DISABLED);
    }
  }

  if (control.slider != nullptr) {
    if (!control.available) {
      lv_obj_add_state(control.slider, LV_STATE_DISABLED);
      lv_label_set_text(control.value_label, "--");
      return;
    }
    lv_obj_clear_state(control.slider, LV_STATE_DISABLED);
    // Home Assistant leaves brightness unset while the light is off.
    int value = 0;
    if (on && std::isfinite(control.brightness)) {
      value = static_cast<int>(std::round(
          std::max(0.0f, std::min(1.0f, control.brightness)) * 100.0f));
    }
    // Don't yank the knob out from under a finger mid-drag.
    if (!lv_obj_has_state(control.slider, LV_STATE_PRESSED)) {
      lv_slider_set_value(control.slider, value, LV_ANIM_OFF);
    }
    char text[8];
    if (on) {
      snprintf(text, sizeof(text), "%d%%", value);
    } else {
      snprintf(text, sizeof(text), "Off");
    }
    lv_label_set_text(control.value_label, text);
  }
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

void ui_add_control(uint8_t index, UiControlType type, const char *name,
                    const char *entity_id) {
  if (index >= theme::kMaxControls) {
    return;
  }
  control_count_ = std::max<uint8_t>(control_count_, index + 1);
  auto &control = controls_[index];
  control.type = type;
  control.icon_kind =
      control_icon_kind(type == UiControlType::kAction, entity_id);
  control.name = name == nullptr ? "" : name;
  control.available = type == UiControlType::kAction;
}

static void style_switch(lv_obj_t *toggle_switch) {
  lv_obj_set_style_bg_color(toggle_switch, lv_color_hex(kColorSwitchTrack),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(toggle_switch, lv_color_hex(kColorSwitchTrack),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(toggle_switch,
                            lv_color_hex(theme::kColorPrimary),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(toggle_switch,
                            lv_color_hex(theme::kColorSecondary),
                            LV_PART_KNOB);
  lv_obj_set_style_bg_color(toggle_switch,
                            lv_color_hex(theme::kColorAccent),
                            LV_PART_KNOB | LV_STATE_CHECKED);
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
  static constexpr int16_t kCardPadding = 12;
  static constexpr int16_t kGapX = 20;
  static constexpr int16_t kGapY = 15;
  static constexpr int16_t kStartX = 20;
  static constexpr int16_t kStartY = 95;
  // Vertical centre of the icon row, relative to the card's content centre.
  static constexpr int16_t kIconRowY = -6;

  for (uint8_t i = 0; i < control_count_; ++i) {
    auto &control = controls_[i];
    const int16_t column = i % 3;
    const int16_t row = i / 3;
    void *user_data = reinterpret_cast<void *>(static_cast<uintptr_t>(i));
    control.card = lv_button_create(parent);
    lv_obj_set_pos(control.card,
                   kStartX + column * (kCardWidth + kGapX),
                   kStartY + row * (kCardHeight + kGapY));
    lv_obj_set_size(control.card, kCardWidth, kCardHeight);
    lv_obj_set_style_radius(control.card, theme::kButtonRadius, 0);
    lv_obj_set_style_border_width(control.card, 0, 0);
    lv_obj_set_style_pad_all(control.card, kCardPadding, 0);
    lv_obj_clear_flag(control.card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(control.card, control_clicked_cb, LV_EVENT_CLICKED,
                        user_data);

    lv_obj_t *name_label = lv_label_create(control.card);
    lv_obj_set_width(name_label, kCardWidth - 2 * kCardPadding);
    lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(
        name_label, lv_color_hex(theme::kColorPrimary), 0);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(name_label, control.name.c_str());

    control.icon_label = lv_label_create(control.card);
    lv_obj_set_style_text_font(control.icon_label, &icon_font_56, 0);

    if (control.type == UiControlType::kAction) {
      // No state to show: just a centred "tap me" icon.
      lv_obj_align(control.icon_label, LV_ALIGN_CENTER, 0, 14);
      render_control(control);
      continue;
    }

    lv_obj_align(control.icon_label, LV_ALIGN_LEFT_MID, 8, kIconRowY);

    control.state_label = lv_label_create(control.card);
    lv_obj_set_width(control.state_label, 130);
    lv_obj_align(control.state_label, LV_ALIGN_RIGHT_MID, -4, kIconRowY);
    lv_obj_set_style_text_align(control.state_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(
        control.state_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(
        control.state_label, lv_color_hex(theme::kColorPrimary), 0);

    if (control.type == UiControlType::kDimmer) {
      control.slider = lv_slider_create(control.card);
      lv_obj_set_size(control.slider, kCardWidth - 2 * kCardPadding - 62, 28);
      lv_obj_align(control.slider, LV_ALIGN_BOTTOM_LEFT, 8, -6);
      lv_slider_set_range(control.slider, 0, 100);
      lv_obj_add_event_cb(control.slider, dimmer_released_cb,
                          LV_EVENT_RELEASED, user_data);

      control.value_label = lv_label_create(control.card);
      lv_obj_align(control.value_label, LV_ALIGN_BOTTOM_RIGHT, 0, -8);
      lv_obj_set_style_text_font(
          control.value_label, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(
          control.value_label, lv_color_hex(theme::kColorPrimary), 0);
    } else {
      control.toggle_switch = lv_switch_create(control.card);
      lv_obj_set_size(control.toggle_switch, 84, 40);
      lv_obj_align(control.toggle_switch, LV_ALIGN_BOTTOM_MID, 0, 0);
      // Display only: taps fall through to the card, which asks Home
      // Assistant to toggle; the switch then follows the reported state.
      lv_obj_clear_flag(control.toggle_switch, LV_OBJ_FLAG_CLICKABLE);
      style_switch(control.toggle_switch);
    }
    render_control(control);
  }
}

void ui_update_control_state(uint8_t index, const char *state,
                             bool available) {
  if (index >= control_count_) {
    return;
  }
  auto &control = controls_[index];
  control.available = available;
  control.active = available && state != nullptr && strcmp(state, "on") == 0;
  render_control(control);
}

void ui_update_control_brightness(uint8_t index, float brightness) {
  if (index >= control_count_) {
    return;
  }
  auto &control = controls_[index];
  control.brightness = brightness;
  render_control(control);
}

}  // namespace alarmclock

#endif  // UNIT_TEST
