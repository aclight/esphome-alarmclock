// Material Design Icons glyphs available in icon_font_36 / icon_font_56.
// Regenerate the fonts with scripts/gen_icon_font.py when adding glyphs.
#ifndef UI_ICONS_H
#define UI_ICONS_H

#include <cstdint>
#include <cstring>

#ifndef UNIT_TEST
#include "lvgl.h"

LV_FONT_DECLARE(icon_font_36);
LV_FONT_DECLARE(icon_font_56);
#endif  // UNIT_TEST

namespace alarmclock {
namespace icons {

// UTF-8 encodings of the MDI codepoints.
static constexpr const char *kFan = "\xF3\xB0\x88\x90";                // U+F0210
static constexpr const char *kFanOff = "\xF3\xB0\xA0\x9D";             // U+F081D
static constexpr const char *kLightbulb = "\xF3\xB0\x8C\xB5";          // U+F0335
static constexpr const char *kLightbulbOutline = "\xF3\xB0\x8C\xB6";   // U+F0336
static constexpr const char *kPower = "\xF3\xB0\x90\xA5";              // U+F0425
static constexpr const char *kPowerPlug = "\xF3\xB0\x9A\xA5";          // U+F06A5
static constexpr const char *kPowerPlugOff = "\xF3\xB0\x9A\xA6";       // U+F06A6
static constexpr const char *kSnowflake = "\xF3\xB0\x9C\x97";          // U+F0717
static constexpr const char *kGestureTapButton = "\xF3\xB1\x8A\xA8";   // U+F12A8

}  // namespace icons

enum class ControlIconKind : uint8_t {
  kPower,
  kLight,
  kFan,
  kSwitch,
  kAction,
};

// Picks a controls-page icon from the entity's domain.
inline ControlIconKind control_icon_kind(bool is_action,
                                         const char *entity_id) {
  if (is_action) {
    return ControlIconKind::kAction;
  }
  if (entity_id == nullptr) {
    return ControlIconKind::kPower;
  }
  if (strncmp(entity_id, "light.", 6) == 0) {
    return ControlIconKind::kLight;
  }
  if (strncmp(entity_id, "fan.", 4) == 0) {
    return ControlIconKind::kFan;
  }
  if (strncmp(entity_id, "switch.", 7) == 0) {
    return ControlIconKind::kSwitch;
  }
  return ControlIconKind::kPower;
}

inline const char *control_icon(ControlIconKind kind, bool on) {
  switch (kind) {
    case ControlIconKind::kLight:
      return on ? icons::kLightbulb : icons::kLightbulbOutline;
    case ControlIconKind::kFan:
      return on ? icons::kFan : icons::kFanOff;
    case ControlIconKind::kSwitch:
      return on ? icons::kPowerPlug : icons::kPowerPlugOff;
    case ControlIconKind::kAction:
      return icons::kGestureTapButton;
    case ControlIconKind::kPower:
    default:
      return icons::kPower;
  }
}

}  // namespace alarmclock

#endif  // UI_ICONS_H
