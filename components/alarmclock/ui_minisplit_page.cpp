// Bedroom minisplit controls page.
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

enum class PickerKind : uint8_t {
  kNone,
  kHvac,
  kFan,
  kHorizontalVane,
  kVerticalVane,
};

static std::string hvac_modes_[theme::kMaxClimateOptions];
static std::string fan_modes_[theme::kMaxClimateOptions];
static uint8_t hvac_mode_count_ = 0;
static uint8_t fan_mode_count_ = 0;
static float min_temperature_ = 60.0f;
static float max_temperature_ = 88.0f;
static float temperature_step_ = 1.0f;
static std::string temperature_unit_ = "°F";

static lv_obj_t *page_ = nullptr;
static lv_obj_t *current_temperature_label_ = nullptr;
static lv_obj_t *target_temperature_label_ = nullptr;
static lv_obj_t *status_label_ = nullptr;
static lv_obj_t *mode_button_ = nullptr;
static lv_obj_t *mode_label_ = nullptr;
static lv_obj_t *fan_button_ = nullptr;
static lv_obj_t *fan_label_ = nullptr;
static lv_obj_t *horizontal_button_ = nullptr;
static lv_obj_t *horizontal_label_ = nullptr;
static lv_obj_t *vertical_button_ = nullptr;
static lv_obj_t *vertical_label_ = nullptr;
static lv_obj_t *power_button_ = nullptr;
static lv_obj_t *temperature_down_button_ = nullptr;
static lv_obj_t *temperature_up_button_ = nullptr;

static lv_obj_t *picker_overlay_ = nullptr;
static lv_obj_t *picker_title_ = nullptr;
static lv_obj_t *picker_option_buttons_[theme::kMaxClimateOptions] = {};
static lv_obj_t *picker_option_labels_[theme::kMaxClimateOptions] = {};
static lv_obj_t *vane_previous_button_ = nullptr;
static lv_obj_t *vane_next_button_ = nullptr;
static lv_obj_t *vane_state_label_ = nullptr;
static PickerKind picker_kind_ = PickerKind::kNone;

static std::string climate_mode_;
static std::string climate_fan_mode_;
static std::string horizontal_vane_;
static std::string vertical_vane_;
static bool climate_available_ = false;

static const char *display_mode_name(const char *mode) {
  if (mode == nullptr || mode[0] == '\0') {
    return "--";
  }
  if (strcmp(mode, "heat_cool") == 0) {
    return "Auto";
  }
  if (strcmp(mode, "fan_only") == 0) {
    return "Fan";
  }
  return mode;
}

static lv_obj_t *create_control_button(lv_obj_t *parent, int16_t x, int16_t y,
                                       int16_t width, int16_t height,
                                       lv_event_cb_t callback) {
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, theme::kButtonRadius, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_bg_color(
      button, lv_color_hex(theme::kColorMuted), 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
  return button;
}

static lv_obj_t *create_button_label(lv_obj_t *button, const char *text,
                                     const lv_font_t *font) {
  lv_obj_t *label = lv_label_create(button);
  lv_obj_center(label);
  lv_obj_set_width(label, lv_pct(95));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(
      label, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_label_set_text(label, text);
  return label;
}

static void close_picker() {
  picker_kind_ = PickerKind::kNone;
  if (picker_overlay_ != nullptr) {
    lv_obj_add_flag(picker_overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void picker_close_cb(lv_event_t *event) {
  (void) event;
  close_picker();
}

void ui_close_minisplit_picker() {
  close_picker();
}

static void picker_option_cb(lv_event_t *event) {
  const uint8_t index =
      static_cast<uint8_t>(reinterpret_cast<uintptr_t>(
          lv_event_get_user_data(event)));
  const auto &callbacks = ui_get_callbacks();
  if (picker_kind_ == PickerKind::kHvac && index < hvac_mode_count_ &&
      callbacks.on_minisplit_hvac_mode != nullptr) {
    callbacks.on_minisplit_hvac_mode(hvac_modes_[index].c_str());
  } else if (picker_kind_ == PickerKind::kFan &&
             index < fan_mode_count_ &&
             callbacks.on_minisplit_fan_mode != nullptr) {
    callbacks.on_minisplit_fan_mode(fan_modes_[index].c_str());
  }
  close_picker();
}

static void vane_step_cb(lv_event_t *event) {
  const bool next =
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)) != 0;
  const bool horizontal = picker_kind_ == PickerKind::kHorizontalVane;
  const auto &callbacks = ui_get_callbacks();
  if (callbacks.on_minisplit_vane_step != nullptr) {
    callbacks.on_minisplit_vane_step(horizontal, next);
  }
}

static void show_picker(PickerKind kind) {
  picker_kind_ = kind;
  lv_obj_clear_flag(picker_overlay_, LV_OBJ_FLAG_HIDDEN);
  const bool vane = kind == PickerKind::kHorizontalVane ||
                    kind == PickerKind::kVerticalVane;
  const uint8_t count =
      kind == PickerKind::kHvac ? hvac_mode_count_ : fan_mode_count_;
  const auto &options =
      kind == PickerKind::kHvac ? hvac_modes_ : fan_modes_;

  const char *title = "Select";
  if (kind == PickerKind::kHvac) {
    title = "HVAC Mode";
  } else if (kind == PickerKind::kFan) {
    title = "Fan Speed";
  } else if (kind == PickerKind::kHorizontalVane) {
    title = "Horizontal Vane";
  } else if (kind == PickerKind::kVerticalVane) {
    title = "Vertical Vane";
  }
  lv_label_set_text(picker_title_, title);

  for (uint8_t i = 0; i < theme::kMaxClimateOptions; ++i) {
    if (!vane && i < count) {
      lv_label_set_text(
          picker_option_labels_[i],
          kind == PickerKind::kHvac
              ? display_mode_name(options[i].c_str())
              : options[i].c_str());
      lv_obj_clear_flag(picker_option_buttons_[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(picker_option_buttons_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (vane) {
    lv_label_set_text(
        vane_state_label_,
        kind == PickerKind::kHorizontalVane
            ? (horizontal_vane_.empty() ? "--" : horizontal_vane_.c_str())
            : (vertical_vane_.empty() ? "--" : vertical_vane_.c_str()));
    lv_obj_clear_flag(vane_previous_button_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(vane_next_button_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(vane_state_label_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(vane_previous_button_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(vane_next_button_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(vane_state_label_, LV_OBJ_FLAG_HIDDEN);
  }
}

static void mode_button_cb(lv_event_t *event) {
  (void) event;
  if (climate_available_) {
    show_picker(PickerKind::kHvac);
  }
}

static void fan_button_cb(lv_event_t *event) {
  (void) event;
  if (climate_available_) {
    show_picker(PickerKind::kFan);
  }
}

static void horizontal_button_cb(lv_event_t *event) {
  (void) event;
  if (climate_available_) {
    show_picker(PickerKind::kHorizontalVane);
  }
}

static void vertical_button_cb(lv_event_t *event) {
  (void) event;
  if (climate_available_) {
    show_picker(PickerKind::kVerticalVane);
  }
}

static void power_button_cb(lv_event_t *event) {
  (void) event;
  const auto &callbacks = ui_get_callbacks();
  if (climate_available_ && callbacks.on_minisplit_power != nullptr) {
    callbacks.on_minisplit_power();
  }
}

static void temperature_button_cb(lv_event_t *event) {
  const intptr_t direction =
      static_cast<intptr_t>(reinterpret_cast<uintptr_t>(
          lv_event_get_user_data(event)));
  const auto &callbacks = ui_get_callbacks();
  if (climate_available_ &&
      callbacks.on_minisplit_temperature_change != nullptr) {
    callbacks.on_minisplit_temperature_change(
        direction == 1 ? 1.0f : -1.0f);
  }
}

void ui_set_minisplit_options(const char *const *hvac_modes,
                              uint8_t hvac_mode_count,
                              const char *const *fan_modes,
                              uint8_t fan_mode_count,
                              float min_temperature,
                              float max_temperature,
                              float temperature_step,
                              const char *temperature_unit) {
  hvac_mode_count_ =
      std::min<uint8_t>(hvac_mode_count, theme::kMaxClimateOptions);
  fan_mode_count_ =
      std::min<uint8_t>(fan_mode_count, theme::kMaxClimateOptions);
  for (uint8_t i = 0; i < hvac_mode_count_; ++i) {
    hvac_modes_[i] = hvac_modes[i] == nullptr ? "" : hvac_modes[i];
  }
  for (uint8_t i = 0; i < fan_mode_count_; ++i) {
    fan_modes_[i] = fan_modes[i] == nullptr ? "" : fan_modes[i];
  }
  min_temperature_ = min_temperature;
  max_temperature_ = max_temperature;
  temperature_step_ = temperature_step;
  temperature_unit_ =
      temperature_unit == nullptr ? "" : temperature_unit;
}

void ui_build_minisplit_page(lv_obj_t *parent) {
  page_ = parent;
  ui_create_page_header(parent, "Bedroom");

  current_temperature_label_ = lv_label_create(parent);
  lv_obj_set_pos(current_temperature_label_, 35, 120);
  lv_obj_set_width(current_temperature_label_, 240);
  lv_obj_set_style_text_align(
      current_temperature_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(
      current_temperature_label_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(
      current_temperature_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(current_temperature_label_, "--");

  lv_obj_t *current_caption = lv_label_create(parent);
  lv_obj_set_pos(current_caption, 35, 180);
  lv_obj_set_width(current_caption, 240);
  lv_obj_set_style_text_align(current_caption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(
      current_caption, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(
      current_caption, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(current_caption, "Current");

  status_label_ = lv_label_create(parent);
  lv_obj_set_pos(status_label_, 35, 220);
  lv_obj_set_width(status_label_, 240);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(
      status_label_, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(status_label_, "Unavailable");

  target_temperature_label_ = lv_label_create(parent);
  lv_obj_set_pos(target_temperature_label_, 290, 120);
  lv_obj_set_width(target_temperature_label_, 220);
  lv_obj_set_style_text_align(
      target_temperature_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(
      target_temperature_label_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(
      target_temperature_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_text(target_temperature_label_, "--");

  lv_obj_t *target_caption = lv_label_create(parent);
  lv_obj_set_pos(target_caption, 290, 180);
  lv_obj_set_width(target_caption, 220);
  lv_obj_set_style_text_align(target_caption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(target_caption, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(
      target_caption, lv_color_hex(theme::kColorSecondary), 0);
  lv_label_set_text(target_caption, "Target");

  temperature_down_button_ =
      create_control_button(parent, 300, 220, 90, 70,
                            temperature_button_cb);
  lv_obj_remove_event_cb(temperature_down_button_, temperature_button_cb);
  lv_obj_add_event_cb(
      temperature_down_button_, temperature_button_cb, LV_EVENT_CLICKED,
      reinterpret_cast<void *>(static_cast<uintptr_t>(0)));
  create_button_label(
      temperature_down_button_, LV_SYMBOL_MINUS, &lv_font_montserrat_32);

  temperature_up_button_ =
      create_control_button(parent, 410, 220, 90, 70,
                            temperature_button_cb);
  lv_obj_remove_event_cb(temperature_up_button_, temperature_button_cb);
  lv_obj_add_event_cb(
      temperature_up_button_, temperature_button_cb, LV_EVENT_CLICKED,
      reinterpret_cast<void *>(static_cast<uintptr_t>(1)));
  create_button_label(
      temperature_up_button_, LV_SYMBOL_PLUS, &lv_font_montserrat_32);

  power_button_ =
      create_control_button(parent, 555, 105, 210, 65, power_button_cb);
  create_button_label(power_button_, "Power", &lv_font_montserrat_24);
  mode_button_ =
      create_control_button(parent, 555, 180, 210, 55, mode_button_cb);
  mode_label_ =
      create_button_label(mode_button_, "Mode: --", &lv_font_montserrat_20);
  fan_button_ =
      create_control_button(parent, 555, 245, 210, 55, fan_button_cb);
  fan_label_ =
      create_button_label(fan_button_, "Fan: --", &lv_font_montserrat_20);
  horizontal_button_ =
      create_control_button(parent, 555, 310, 210, 55, horizontal_button_cb);
  horizontal_label_ = create_button_label(
      horizontal_button_, "Horizontal: --", &lv_font_montserrat_18);
  vertical_button_ =
      create_control_button(parent, 555, 375, 210, 55, vertical_button_cb);
  vertical_label_ = create_button_label(
      vertical_button_, "Vertical: --", &lv_font_montserrat_18);

  picker_overlay_ = lv_obj_create(parent);
  lv_obj_set_pos(picker_overlay_, 0, theme::kPageHeaderHeight);
  lv_obj_set_size(
      picker_overlay_, theme::kScreenWidth,
      theme::kScreenHeight - theme::kPageHeaderHeight);
  lv_obj_set_style_bg_color(
      picker_overlay_, lv_color_hex(0x101010), 0);
  lv_obj_set_style_bg_opa(picker_overlay_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(picker_overlay_, 0, 0);
  lv_obj_set_style_radius(picker_overlay_, 0, 0);
  lv_obj_clear_flag(picker_overlay_, LV_OBJ_FLAG_SCROLLABLE);

  picker_title_ = lv_label_create(picker_overlay_);
  lv_obj_align(picker_title_, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_text_font(
      picker_title_, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(
      picker_title_, lv_color_hex(theme::kColorPrimary), 0);

  lv_obj_t *close_button =
      create_control_button(picker_overlay_, 720, 10, 60, 55,
                            picker_close_cb);
  create_button_label(close_button, LV_SYMBOL_CLOSE, &lv_font_montserrat_24);

  for (uint8_t i = 0; i < theme::kMaxClimateOptions; ++i) {
    const int16_t column = i % 4;
    const int16_t row = i / 4;
    picker_option_buttons_[i] =
        create_control_button(picker_overlay_, 25 + column * 195,
                              90 + row * 110, 170, 85,
                              picker_option_cb);
    lv_obj_remove_event_cb(
        picker_option_buttons_[i], picker_option_cb);
    lv_obj_add_event_cb(
        picker_option_buttons_[i], picker_option_cb, LV_EVENT_CLICKED,
        reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
    picker_option_labels_[i] = create_button_label(
        picker_option_buttons_[i], "", &lv_font_montserrat_20);
  }

  vane_previous_button_ =
      create_control_button(picker_overlay_, 90, 155, 170, 90,
                            vane_step_cb);
  lv_obj_remove_event_cb(vane_previous_button_, vane_step_cb);
  lv_obj_add_event_cb(
      vane_previous_button_, vane_step_cb, LV_EVENT_CLICKED,
      reinterpret_cast<void *>(static_cast<uintptr_t>(0)));
  create_button_label(
      vane_previous_button_, LV_SYMBOL_LEFT, &lv_font_montserrat_32);
  vane_next_button_ =
      create_control_button(picker_overlay_, 540, 155, 170, 90,
                            vane_step_cb);
  lv_obj_remove_event_cb(vane_next_button_, vane_step_cb);
  lv_obj_add_event_cb(
      vane_next_button_, vane_step_cb, LV_EVENT_CLICKED,
      reinterpret_cast<void *>(static_cast<uintptr_t>(1)));
  create_button_label(
      vane_next_button_, LV_SYMBOL_RIGHT, &lv_font_montserrat_32);
  vane_state_label_ = lv_label_create(picker_overlay_);
  lv_obj_align(vane_state_label_, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_width(vane_state_label_, 240);
  lv_obj_set_style_text_align(vane_state_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(
      vane_state_label_, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(
      vane_state_label_, lv_color_hex(theme::kColorPrimary), 0);
  lv_label_set_long_mode(vane_state_label_, LV_LABEL_LONG_DOT);
  lv_obj_add_flag(picker_overlay_, LV_OBJ_FLAG_HIDDEN);
}

static void set_control_enabled(lv_obj_t *control, bool enabled) {
  if (control == nullptr) {
    return;
  }
  if (enabled) {
    lv_obj_clear_state(control, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(control, LV_STATE_DISABLED);
  }
}

void ui_update_minisplit_state(const char *mode, const char *action,
                               float current_temperature,
                               float target_temperature,
                               const char *fan_mode,
                               const char *horizontal_vane,
                               const char *vertical_vane, bool available) {
  climate_available_ = available;
  climate_mode_ = mode == nullptr ? "" : mode;
  climate_fan_mode_ = fan_mode == nullptr ? "" : fan_mode;
  horizontal_vane_ = horizontal_vane == nullptr ? "" : horizontal_vane;
  vertical_vane_ = vertical_vane == nullptr ? "" : vertical_vane;
  if (page_ == nullptr) {
    return;
  }

  char text[48];
  if (available && std::isfinite(current_temperature)) {
    snprintf(text, sizeof(text), "%.1f%s", current_temperature,
             temperature_unit_.c_str());
  } else {
    snprintf(text, sizeof(text), "--%s", temperature_unit_.c_str());
  }
  lv_label_set_text(current_temperature_label_, text);

  if (available && std::isfinite(target_temperature)) {
    snprintf(text, sizeof(text), "%.1f%s", target_temperature,
             temperature_unit_.c_str());
  } else {
    snprintf(text, sizeof(text), "--%s", temperature_unit_.c_str());
  }
  lv_label_set_text(target_temperature_label_, text);
  lv_label_set_text(
      status_label_,
      available ? ((action != nullptr && action[0] != '\0') ? action : "Idle")
                : "Unavailable");

  snprintf(text, sizeof(text), "Mode: %s",
           display_mode_name(climate_mode_.c_str()));
  lv_label_set_text(mode_label_, text);
  snprintf(text, sizeof(text), "Fan: %s",
           climate_fan_mode_.empty() ? "--" : climate_fan_mode_.c_str());
  lv_label_set_text(fan_label_, text);
  snprintf(text, sizeof(text), "Horizontal: %s",
           horizontal_vane_.empty() ? "--" : horizontal_vane_.c_str());
  lv_label_set_text(horizontal_label_, text);
  snprintf(text, sizeof(text), "Vertical: %s",
           vertical_vane_.empty() ? "--" : vertical_vane_.c_str());
  lv_label_set_text(vertical_label_, text);

  const bool is_on = available && climate_mode_ != "off";
  lv_obj_set_style_bg_color(
      power_button_,
      lv_color_hex(is_on ? theme::kColorAccent : theme::kColorMuted), 0);
  set_control_enabled(power_button_, available);
  set_control_enabled(mode_button_, available);
  set_control_enabled(fan_button_, available);
  set_control_enabled(horizontal_button_, available);
  set_control_enabled(vertical_button_, available);
  set_control_enabled(
      temperature_down_button_,
      available && std::isfinite(target_temperature) &&
          target_temperature > min_temperature_);
  set_control_enabled(
      temperature_up_button_,
      available && std::isfinite(target_temperature) &&
          target_temperature < max_temperature_);

  if (picker_kind_ == PickerKind::kHorizontalVane) {
    lv_label_set_text(
        vane_state_label_,
        horizontal_vane_.empty() ? "--" : horizontal_vane_.c_str());
  } else if (picker_kind_ == PickerKind::kVerticalVane) {
    lv_label_set_text(
        vane_state_label_,
        vertical_vane_.empty() ? "--" : vertical_vane_.c_str());
  }
}

}  // namespace alarmclock

#endif  // UNIT_TEST
