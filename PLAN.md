# Alarm Clock — Code Review & Fix Plan

> **Previous review (Batch 1–3):** All 18 original issues have been fixed.
> Day-of-week offset, callback registration, sync_ui\_ visibility, pending alarm
> queue, default-alarm persistence, 24h format support, clock_font_120,
> scrollable settings, swipe vertical guard, keyboard, cancel button, day
> button sizing (50px), "Once" label, and page dot updates are all resolved.
>
> This plan covers **new issues** found during a fresh code review (May 2025).

---

## Critical Logic Bugs

### 1. Day-of-week toggle buttons in time picker are broken (double-toggle)
**File:** `ui_time_picker.cpp` — `day_btn_cb` + `LV_OBJ_FLAG_CHECKABLE`

The day buttons have `LV_OBJ_FLAG_CHECKABLE` (line ~278), which causes LVGL to automatically toggle `LV_STATE_CHECKED` on click *before* the event callback fires. The `day_btn_cb` callback then manually toggles the state again, reverting the change. Net effect: clicking a day button does nothing.

**Effect:** Users cannot select or deselect repeat days when editing an alarm. All alarms saved from the time picker retain whatever days were set when the picker opened.

**Fix:** Remove the manual toggle logic from `day_btn_cb`. Since `LV_OBJ_FLAG_CHECKABLE` handles the state automatically, the callback body should be empty (or removed entirely, and the `lv_obj_add_event_cb` call deleted). Alternatively, remove `LV_OBJ_FLAG_CHECKABLE` and keep the manual toggle.

---

### 2. Snooze re-fire does not update firing overlay or restart animation
**File:** `alarmclock.cpp`, `loop()` — snooze tick handler (line ~186)

When a snooze expires, only `start_alarm_sound_()`, `ui_show_firing_overlay()`, and the HA event are called. Missing:
- `ui_firing_update_time()` — the overlay still shows the original trigger time, not the current time
- `ui_firing_update_label()` — label is not re-applied
- `ui_firing_start_animation()` — the pulsing animation is not restarted

**Effect:** After a snooze, the firing overlay appears but shows a stale time and has no pulsing animation, making it look broken.

**Fix:** Add the three missing calls to the `tick_snooze()` handler, matching the pattern used in `check_alarms_()` when an alarm first fires. This requires the current hour/minute, which can be obtained from the time source or stored when the alarm first triggered.

---

### 3. NVS flash wear from unbounded writes on slider drag
**Files:** `ui_settings_page.cpp`, `alarmclock.cpp`

Volume and brightness sliders fire `LV_EVENT_VALUE_CHANGED` continuously during drag. Each event calls `set_volume()` / `set_brightness()`, which call `save_settings_to_storage_()`, writing to NVS flash on every pixel of slider movement. ESP-IDF NVS has limited write endurance (~100K cycles per sector).

**Effect:** Heavy slider use can prematurely wear out the NVS flash partition, eventually causing `ESP_ERR_NVS_NO_FREE_PAGES` and data loss.

**Fix:** Debounce NVS writes. Options:
- Only save on `LV_EVENT_RELEASED` (slider release), not `LV_EVENT_VALUE_CHANGED`.
- Add a timer-based debounce (e.g., save at most once per 2 seconds, with a pending-save flag).
- Split the apply (immediate backlight/volume change) from the persist (deferred NVS write).

---

## UI Bugs

### 4. Page indicator dots only visible on clock page
**File:** `ui_clock_page.cpp` — `ui_build_clock_page()`

The `page_dots_[]` widgets are created as children of the clock page container (`parent = pages_[kPageClock]`). When the user navigates to the alarm or settings page, the clock page is hidden (`LV_OBJ_FLAG_HIDDEN`), and the dots disappear.

**Effect:** Page indicator dots are invisible on the alarm and settings pages, providing no navigation context.

**Fix:** Move dot creation out of `ui_build_clock_page()` and into `ui_init()`, creating them as children of the screen root (`lv_scr_act()`) with a high Z-order so they remain visible on all pages. Alternatively, create duplicate dot sets on each page.

---

### 5. "Add Alarm" button has no click handler
**File:** `ui_alarm_page.cpp` — `add_btn_` (line ~130)

The button is created and styled but has no `lv_obj_add_event_cb` attached. The existing TODO says "Implement add-alarm flow."

**Effect:** Users cannot create new alarms from the touch UI. They can only edit the default alarm or alarms created via Home Assistant.

**Fix:** Add a click handler that finds the first unused alarm slot (hour=0, minute=0, days=0, enabled=false) and opens the time picker for it with defaults. If all slots are full, show a brief message or disable the button.

---

### 6. Time picker always uses 12h roller format
**File:** `ui_time_picker.cpp`

The hour roller always shows hours 1–12 with a separate AM/PM roller, regardless of the user's `time_format_24h` setting. This is confusing when the rest of the UI shows 24h format.

**Effect:** Users in 24h mode see "14:30" on the clock but must select "2 PM" in the picker.

**Fix:** When `time_format_24h` is true:
- Change the hour roller options to "00\n01\n...\n23" (24 entries).
- Hide the AM/PM roller.
- Adjust the confirm callback to read the hour directly (no 12→24 conversion).
Pass the `time_format_24h` flag to `ui_show_time_picker()` or store it globally in the UI module.

---

### 7. Day-of-week button labels are ambiguous
**File:** `ui_time_picker.cpp` — `kDayLetters[]`

Sunday and Saturday both show "S"; Tuesday and Thursday both show "T". Users cannot distinguish them.

**Effect:** Users may select the wrong days, causing alarms to fire on unintended days.

**Fix:** Use two-letter abbreviations: `{"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}`. Increase button width from 50px to ~56px to accommodate, or use a slightly smaller font. At 7 × (56 + 8) = 448px, this still fits within the 700px picker.

---

### 8. Alarm row content overflows container at bottom
**File:** `ui_alarm_page.cpp`

Row height is 80px. Widget positions (from `LV_ALIGN_LEFT_MID`):
- `time_label`: y=-15 (center at 25, ~28px font → spans 11–39)
- `alarm_label`: y=15 (center at 55, ~18px font → spans 46–64)  
- `days_label`: y=32 (center at 72, ~18px font → spans 63–81)

The days label extends 1px past the 80px container, causing clipping. When both a label and days are shown, the vertical space is tight and may overlap.

**Fix:** Either:
- Increase `kAlarmRowHeight` to 90–95px (and adjust `kAlarmListStartY` or reduce `kAlarmRowGap`).
- Move `days_label` up (y=25) and `alarm_label` up (y=10), compacting the layout.
- Use a two-line layout: time + label on one line, days on the second line.

---

## Logic / Behavior Issues

### 9. Screen sleep allows up to 50% brightness in bright rooms
**Files:** `alarmclock.cpp` — `check_screen_sleep_()`, `alarmclock.h` — `compute_brightness()`

When the screen sleeps, `user_level` is set to `kSleepUserLevel = 0.0`. With default parameters (`auto_range=0.5, min=0.0, max=1.0`), the brightness window is [0.0, 0.5]. In a bright room (`sensor_factor=1.0`), sleep brightness is 0.5 (50%), which is not dim at all.

**Effect:** The screen doesn't dim much when sleeping in a well-lit room (e.g., sunset light coming through a window), which is distracting and wastes power.

**Fix:** When asleep, ignore or attenuate the sensor factor. Options:
- Use `compute_brightness(0.0, 0.0)` when sleeping (always minimum brightness).
- Apply a sleep multiplier: `bright = compute_brightness(0.0, sensor_factor) * 0.2f`.
- Use a separate, lower `auto_range` for sleep mode (e.g., 0.1 instead of 0.5).

---

### 10. Pending alarm queue only stores one alarm
**File:** `alarmclock.cpp` — `pending_alarm_index_`

If two different alarms match during the same minute while a third is firing/snoozed, only the last matching alarm is stored in `pending_alarm_index_`. The earlier one is silently lost.

**Effect:** With 4 alarm slots, it's unlikely but possible (e.g., two alarms at the same time on the same day). The second queued alarm would overwrite the first.

**Fix (low priority):** Use a small array/bitmask instead of a single index: `uint8_t pending_alarm_mask_ = 0;` with `pending_alarm_mask_ |= (1 << i)`. When processing pending alarms, find the first set bit, clear it, and fire that alarm.

---

### 11. `compute_content_color` is dead code
**File:** `alarmclock.h`

The function is defined and tested but never called from any UI or component code. It was likely intended to dynamically adjust UI text color based on brightness (supplement PWM dimming with content dimming), but integration was never completed.

**Effect:** No functional impact — the function compiles and tests pass, but it has no runtime effect. Text stays white at all brightness levels.

**Fix:** Either:
- Integrate it: call from `update_backlight_()` and pass the result to a new UI function that sets text colors on clock/date labels.
- Remove it: delete the function and its tests to reduce dead code.

---

### 12. Firing overlay does not show current time
**Files:** `alarmclock.cpp` — `check_alarms_()`, `ui_firing_overlay.cpp`

When an alarm fires, `ui_firing_update_time()` is called once with the alarm's hour/minute. The overlay then shows a static time (e.g., "7:00 AM") for the entire firing duration (up to 30 minutes). The main clock face is completely hidden behind the overlay.

**Effect:** Users lose awareness of the current time while the alarm is firing. If they snooze and the alarm re-fires at 7:09, the overlay still shows "7:00 AM" (see also Issue #2).

**Fix:** Update the firing overlay time every second from the YAML interval lambda (alongside the main clock update), or add a secondary "current time" label to the firing overlay.

---

## Documentation / Code Quality

### 13. `compute_brightness` comment examples use wrong default min
**File:** `alarmclock.h` — comment block above `compute_brightness()`

The comment says:
```
// Examples with defaults (auto_range=0.5, min=0.1, max=1.0):
```

But the actual default is `min_brightness = kMinBrightness = 0.0f`, not 0.1. The example values (user_level=0.0 → window [0.10, 0.60]) are wrong.

**Fix:** Update the comment to use `min=0.0`:
```
// Examples with defaults (auto_range=0.5, min=0.0, max=1.0):
//   user_level=1.0 → window [0.50, 1.00]
//   user_level=0.0 → window [0.00, 0.50]
//   user_level=0.5 → window [0.25, 0.75]
```

---

### 14. Namespace split (`alarm_clock` vs `alarmclock`) is confusing
**Files:** `alarm_time.h`, `alarm_state.h` (`namespace alarm_clock`), all other files (`namespace alarmclock`)

The pure-logic headers use `alarm_clock` (with underscore), while the component and UI use `alarmclock` (no underscore). This forces qualified access like `alarm_clock::AlarmTime` throughout the component code and makes it easy to accidentally use the wrong namespace.

**Fix (low priority):** Unify to a single namespace. `alarmclock` (no underscore) is already used by the ESPHome component registration. Rename `alarm_clock` → `alarmclock` in `alarm_time.h`, `alarm_state.h`, and `storage.h`, and update all references.

---

## Summary of Priority

| Priority | Issues |
|----------|--------|
| **P0 — Broken functionality** | #1 (day buttons don't toggle), #2 (snooze re-fire display) |
| **P1 — Data integrity / hardware** | #3 (NVS flash wear) |
| **P2 — Missing features** | #5 (add alarm button), #6 (24h time picker) |
| **P3 — UI correctness** | #4 (page dots), #7 (day labels), #8 (row overflow), #12 (firing time) |
| **P4 — Behavior / polish** | #9 (sleep brightness), #10 (pending queue), #11 (dead code), #13 (comment), #14 (namespace) |

---

## Implementation Batches

### Batch 1 — Critical fixes (P0 + P1): Issues #1, #2, #3

Fix the broken day-of-week buttons, snooze re-fire display, and NVS wear.

**Files touched:**
- `ui_time_picker.cpp` (remove manual toggle from `day_btn_cb`)
- `alarmclock.cpp` (add missing calls in snooze re-fire path; debounce NVS writes)
- `ui_settings_page.cpp` (possibly change slider event from VALUE_CHANGED to RELEASED)

**Tests:**
- Verify day buttons toggle correctly in time picker.
- Verify snooze re-fire shows updated time and restarts pulse animation.
- Verify settings persist correctly but NVS writes are bounded.

---

### Batch 2 — UI fixes (P2 + P3): Issues #4, #5, #6, #7, #8, #12

Fix page dots visibility, implement Add Alarm, add 24h picker mode, improve day labels, fix alarm row layout, and update firing overlay time.

**Files touched:**
- `ui_clock_page.cpp` (move dot creation)
- `ui.cpp` (dots as screen-root children)
- `ui_alarm_page.cpp` (add button handler, fix row height/spacing)
- `ui_time_picker.cpp` (24h roller mode, two-letter day labels)
- `alarmclock.cpp` / YAML interval (update firing overlay time each second)

**Tests:**
- Page dots visible on all three pages.
- Add Alarm button opens time picker for empty slot.
- 24h mode shows 0–23 hour roller without AM/PM.
- Day labels clearly distinguishable (Su, Mo, Tu, We, Th, Fr, Sa).
- No clipping on alarm rows with all fields populated.
- Firing overlay shows current time, not stale trigger time.

---

### Batch 3 — Polish (P4): Issues #9, #10, #11, #13, #14

Lower-priority improvements — sleep brightness, pending alarm robustness, dead code cleanup.

**Files touched:**
- `alarmclock.cpp` (sleep brightness override, pending alarm bitmask)
- `alarmclock.h` (fix comment, remove or integrate `compute_content_color`)
- `alarm_time.h`, `alarm_state.h`, `storage.h` (namespace rename, if desired)

---

### Future (not in this plan)
- WAV/MP3 alarm sounds with custom partition table
- HA sync (restore alarms from HA entities on boot)
- Weather display on clock face
- Sunrise alarm (gradually increase brightness before alarm time)
