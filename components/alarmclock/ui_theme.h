// UI theme constants — colors, sizes, fonts.
// Centralized so the look can be adjusted in one place.
#ifndef UI_THEME_H
#define UI_THEME_H

#include <cstdint>

namespace alarmclock {
namespace theme {

// --- Colors (0xRRGGBB) ---
static constexpr uint32_t kColorBackground = 0x000000;
static constexpr uint32_t kColorPrimary = 0xFFFFFF;     // Main text/icons.
static constexpr uint32_t kColorSecondary = 0xAAAAAA;   // Subtitles, date.
static constexpr uint32_t kColorAccent = 0x4488FF;      // Active toggles, highlights.
static constexpr uint32_t kColorAlarmFiring = 0xFF4444;  // Firing overlay background.
static constexpr uint32_t kColorSnooze = 0xFFAA00;      // Snooze button.
static constexpr uint32_t kColorDismiss = 0x44CC44;     // Dismiss button.
static constexpr uint32_t kColorMuted = 0x333333;       // Disabled/inactive.
static constexpr uint32_t kColorDivider = 0x222222;     // Separator lines.

// --- Dimensions (pixels, for 800x480) ---
static constexpr int16_t kScreenWidth = 800;
static constexpr int16_t kScreenHeight = 480;

// Clock page.
static constexpr int16_t kClockTimeY = -60;    // Time label Y offset from center.
static constexpr int16_t kClockDateY = 60;     // Date label Y offset from center.
static constexpr int16_t kClockAlarmY = 100;   // Next-alarm label Y offset.
static constexpr int16_t kClockPreAlarmY = 140; // Pre-alarm banner Y offset.

// Navigation.
static constexpr int16_t kSwipeThreshold = 80;  // Pixels to trigger page switch.
static constexpr uint16_t kPageAnimDuration = 300;  // ms for page transition.

// Buttons.
static constexpr int16_t kButtonWidth = 300;
static constexpr int16_t kButtonHeight = 110;
static constexpr int16_t kButtonRadius = 12;

// Firing overlay.
static constexpr int16_t kFiringTimeY = -120;
static constexpr int16_t kFiringSnoozeY = 20;
static constexpr int16_t kFiringDismissY = 150;

// --- Pages (indices for navigation) ---
static constexpr uint8_t kPageClock = 0;
static constexpr uint8_t kPageAlarms = 1;
static constexpr uint8_t kPageSettings = 2;
static constexpr uint8_t kPageCount = 3;

}  // namespace theme
}  // namespace alarmclock

#endif  // UI_THEME_H
