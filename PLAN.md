# Alarm Clock — Code Review & Fix Plan

## Critical Logic Bugs

### 1. Day-of-week off-by-one error (alarms fire on wrong day)
**Files:** `alarmclock.yaml` interval lambda, `alarm_time.h`

ESPHome's `ESPTime::day_of_week` is 1-based (1=Sunday … 7=Saturday), but all alarm logic in `alarm_time.h` expects 0-based (0=Sunday … 6=Saturday). The YAML passes `time.day_of_week` directly to `check_alarms_()` without subtracting 1.

**Effect:** Every alarm fires one day late (a Monday alarm fires on Tuesday). Saturday alarms map to `1 << (7 % 7) = kSunday`, so they fire on Sunday instead.

**Fix:** Subtract 1 from `time.day_of_week` before passing to `check_alarms_()` and `ui_update_date()` in the YAML interval lambda, OR adjust the conversion inside `check_alarms_` and `ui_update_date`.

---

### 2. `ui_update_date` shows wrong day name and breaks on Saturday
**File:** `ui_clock_page.cpp`

`kDayNames[]` is 0-indexed (0=Sunday), but receives ESPHome's 1-based `day_of_week`. On Sunday it shows "Monday" (index 1). On Saturday (`day_of_week=7`), the guard `day_of_week > 6` rejects the value, so date display never updates on Saturdays.

**Fix:** Apply the same -1 offset fix from issue #1.

---

### 3. `on_alarm_time_set` overwrites days with 0xFF
**File:** `alarmclock.cpp` (line ~57)

```cpp
instance_->set_alarm(index, hour, minute, 0xFF, true);  // TODO: pass actual days
```

When the time picker confirms, this callback overwrites the user's day selection with "every day" (0xFF).

**Fix:** The time picker already fires `on_alarm_time_set`, `on_alarm_days_set`, and `on_alarm_label_set` as separate callbacks. Change `on_alarm_time_set` to only update hour/minute on the existing alarm, preserving the current `days_of_week`. Alternatively, merge into a single callback that accepts all fields.

---

### 4. `on_alarm_days_set` callback is unimplemented
**File:** `alarmclock.cpp` (line ~61)

The callback body is empty (TODO). Days selected in the time picker are silently discarded.

**Fix:** Implement the callback to call `set_alarm()` (or a new setter) with the days_mask while preserving hour/minute/enabled.

---

### 5. `on_alarm_edit`, `on_alarm_delete`, `on_alarm_label_set` callbacks never registered
**File:** `alarmclock.cpp` (setup, ~line 107)

These callbacks are left as `nullptr` in the `UiCallbacks` struct. Consequences:
- Tapping an alarm row does nothing (time picker never opens).
- Long-pressing an alarm row does nothing (can't delete).
- Label text from the time picker is discarded.

**Fix:** Implement and register handlers for all three:
- `on_alarm_edit` → call `ui_show_time_picker(index, ...)`
- `on_alarm_delete` → clear alarm, hide row, persist
- `on_alarm_label_set` → update `alarms_[index].label`, persist

---

### 6. `sync_ui_()` hides one-shot alarms after reboot
**File:** `alarmclock.cpp` (line ~400)

```cpp
if (alarms_[i].days_of_week != 0) {  // one-shot alarms (==0) never shown!
    ui_update_alarm_row(i, ...);
}
```

One-shot alarms (no days set) are valid enabled alarms, but they're invisible after every boot.

**Fix:** Change condition to `if (alarms_[i].enabled || alarms_[i].days_of_week != 0)` or simply always call `ui_update_alarm_row` for any alarm that has been configured (e.g., check if hour/minute are non-default or `enabled` is true).

---

### 7. Other alarms blocked while one alarm is firing/snoozed
**File:** `alarmclock.cpp`, `check_alarms_()` (line ~298)

```cpp
if (state_machine_.state() != alarm_clock::AlarmState::kIdle) {
    return;  // Already firing or snoozed.
}
```

If alarm A is snoozed for 9 minutes and alarm B is due during that window, alarm B is silently skipped and never fires.

**Fix:** Keep a single state machine but queue missed alarms. When `check_alarms_()` finds a matching alarm while already firing/snoozed, store its index in a "pending" queue. After the current alarm is dismissed, check the pending queue and immediately fire the next queued alarm if still within its trigger window (same minute).

---

### 8. Default first-boot alarm not persisted to NVS
**File:** `alarmclock.cpp`, `load_from_storage_()` (line ~508)

When no alarms exist in NVS, a default 7:00 AM weekday alarm is created in RAM but never saved. If the device reboots before the user makes a change, the default disappears.

**Fix:** Call `storage_save_alarm(0, alarms_[0])` after creating the default.

---

## Time Format Inconsistency

### 9. Multiple UI functions ignore the 24h/12h setting
**Files:** `alarmclock.h` (`format_next_alarm_text`), `ui_firing_overlay.cpp` (`ui_firing_update_time`), `ui_alarm_page.cpp` (`ui_update_alarm_row`)

These functions hardcode 12-hour format with AM/PM regardless of the user's `time_format_24h_` preference:
- `format_next_alarm_text()` — always shows "7:00 AM (in 6h 30m)"
- `ui_firing_update_time()` — always shows "7:00 AM" on the firing overlay
- `ui_update_alarm_row()` — always shows "7:00 AM" in alarm list

**Fix:** Pass the `time_format_24h` flag to these functions (or use `format_clock_time()` which already handles both formats).

---

## Code/Comment Mismatch

### 10. `compute_content_color` uses quadratic but comment says "sqrt"
**File:** `alarmclock.h` (line ~275)

Comment says "Use sqrt curve so color stays bright longer" but code does `t = brightness * brightness` (quadratic — opposite behavior). The documented example values (brightness 0.5 → ~0xB4) match a sqrt curve, not what's implemented.

**Fix:** Either change to `t = sqrtf(brightness)` to match the documented behavior, or update the comment and example values to reflect the quadratic curve. The sqrt curve is likely the intended behavior (keep text readable longer as backlight dims).

---

## UI Usability Issues

### 11. Clock face time is too small for a bedside clock
**File:** `ui_clock_page.cpp`

The time uses `lv_font_montserrat_48` (48px ≈ 5.6mm on this 217 PPI display). A bedside clock should have digits at least 20-30mm tall (readable at arm's length in low light without glasses).

**Fix:** Use a much larger font — minimum 100px, ideally 120-140px. This requires adding a custom font or using LVGL's font generation. The date and next-alarm labels can shift down accordingly. Consider making the LVGL `boot_page` reference the larger font so it gets compiled in.

---

### 12. Settings page overflows screen and is not scrollable
**File:** `ui_settings_page.cpp`

Content extends to Y≈470+ (time format toggle at Y=440 + control height) on a 480px screen. The page has scrolling disabled (`LV_OBJ_FLAG_SCROLLABLE` cleared in `ui.cpp`). The bottom controls are unreachable or barely visible.

**Fix:** Either:
- Enable scrolling on the settings page only (add `LV_OBJ_FLAG_SCROLLABLE` to the settings page), OR
- Reorganize layout to fit within 480px (consolidate sections, reduce spacing), OR
- Split into two settings sub-pages

---

### 13. Swipe gesture conflicts with sliders and rollers
**File:** `ui.cpp`

The swipe handler captures `LV_EVENT_PRESSED`/`LV_EVENT_RELEASED` on the page container. LVGL events bubble from child to parent, so dragging a horizontal slider can inadvertently trigger a page switch if the drag exceeds 80px.

**Fix:** Add a vertical displacement check — only trigger page switch if horizontal displacement exceeds threshold AND vertical displacement is small (|dy| < 40px). Alternatively, check if the touch started on an interactive widget and suppress swipe in that case.

---

### 14. No on-screen keyboard for alarm label input
**File:** `ui_time_picker.cpp`

The label `lv_textarea` has no attached LVGL keyboard. Without a physical keyboard, users cannot type alarm labels.

**Fix:** Create an `lv_keyboard` object in the time picker. Show it when the textarea gains focus, hide it when done. Alternatively, replace the free-text input with a preset label picker (roller or buttons: "Work", "School", "Nap", "Custom").

---

### 15. Time picker has no visible Cancel button
**File:** `ui_time_picker.cpp`

The only way to cancel editing is tapping the semi-transparent backdrop. This is not discoverable, especially in low light or for users unfamiliar with the UI pattern.

**Fix:** Add an explicit "Cancel" button (alongside "Save" and "Delete") that calls `ui_hide_time_picker()`.

---

### 16. Day-of-week buttons too small (44px < 48px minimum)
**File:** `ui_time_picker.cpp`

`kDayBtnSize = 44` is below the recommended 48dp minimum touch target for accessibility. At 217 PPI, 44px ≈ 5.1mm — hard to tap accurately when groggy.

**Fix:** Increase to 48-52px. With 7 buttons at (52+8)px spacing = 420px, this still fits within the 700px picker width.

---

### 17. One-shot alarms show no indicator in alarm list
**File:** `ui_alarm_page.cpp`

When `days_of_week == 0`, the days label is empty, giving no visual cue that this is a "fire once" alarm vs. a misconfigured alarm.

**Fix:** When `days_mask == 0`, display "Once" or "One-time" in the days label.

---

### 18. Page dots never update on page change
**File:** `ui_clock_page.cpp`

`ui_update_page_dots()` is defined but never called. The dots always show page 0 (clock) as active regardless of which page is visible.

**Fix:** Call `ui_update_page_dots(page_index)` from `ui_show_page()` in `ui.cpp` after the transition starts.

---

### 19. Small fonts on alarm rows hurt readability
**File:** `ui_alarm_page.cpp`

The alarm label and days use `lv_font_montserrat_14` (14px ≈ 1.6mm). This is very hard to read in low light or without glasses.

**Fix:** Increase to `montserrat_18` or `montserrat_20` for secondary text on alarm rows. Adjust row height if needed to accommodate.

---

### 20. Alarm "Add" button may overlap last alarm row
**File:** `ui_alarm_page.cpp`

Four alarm rows: start Y=60, each row 90px + 10px gap = 400px. Last row ends at Y=460. "Add Alarm" button is at `BOTTOM_MID, 0, -30` = Y=400 (480-30-50=400). The button (50px tall, centered at Y=400) overlaps with the 4th alarm row.

**Fix:** Recalculate layout: reduce row heights, reduce spacing, or position the Add button conditionally (only show when fewer than kMaxAlarms are configured). Alternatively, offset alarm list to start higher or make the page scrollable.

---

### 21. Firing overlay buttons could be larger
**File:** `ui_firing_overlay.cpp`, `ui_theme.h`

Snooze/Dismiss buttons are 200×80px with montserrat_24 text. While acceptable, for a panic-press scenario (alarm blaring, user half-asleep), larger buttons (280×90+) and larger text (montserrat_28 or _32) would significantly improve usability.

**Fix:** Increase `kButtonWidth` to 260-280 and `kButtonHeight` to 90. Increase button label font to `montserrat_28`. Adjust Y positions to maintain spacing.

---

### 22. No visual indication of which alarm is currently firing
**File:** `ui_alarm_page.cpp`

When an alarm is firing or snoozed, its row in the alarm list looks the same as any other enabled alarm. Users navigating to the alarm page can't tell which alarm triggered.

**Fix:** Highlight the firing alarm's row (e.g., change background color to a muted red/orange, or add a pulsing indicator).

---

## Lower Priority / Polish

### 23. Slider knob size too small for bedside use
**File:** `ui_settings_page.cpp`

Default LVGL slider knobs are small. For a groggy user, the knob should be enlarged.

**Fix:** Set `lv_obj_set_style_pad_all(slider, 8, LV_PART_KNOB)` or similar to increase the knob hit area.

---

### 24. Volume ramp resets on snooze re-fire
**File:** `alarmclock.cpp`, `start_alarm_sound_()`

When snooze expires and the alarm re-fires, `alarm_sound_start_ms_` resets, causing the volume ramp to restart from 10%. On subsequent snooze-fires, users may want immediate full volume since they've already been woken.

**Fix:** On snooze re-fire (snooze_count > 0), skip the ramp or use a shorter ramp duration.

---

### 25. `kPreAlarmMinutes` should be configurable
**File:** `alarmclock.h`

The pre-alarm notification threshold is hardcoded to 5 minutes. Some users prefer more warning time.

**Fix:** Add to StorageSettings and expose in the settings page (e.g., "0 / 5 / 10 / 15 min" options).

---

## Summary of Priority

| Priority | Issues |
|----------|--------|
| **P0 — Alarms broken** | #1, #2, #3, #4, #5 |
| **P1 — Data loss / silent failures** | #6, #7, #8 |
| **P2 — Wrong display info** | #9, #10, #18 |
| **P3 — Usability (important for bedside clock)** | #11, #12, #13, #14, #15, #16, #20, #21 |
| **P4 — Polish** | #17, #19, #22, #23, #24, #25 |

---

## Implementation Batches

### Batch 1 — Core alarm logic (P0 + P1): Issues #1–8

Fix all alarm logic bugs so alarms fire correctly and the edit flow works.
These are interdependent: #1/#2 share a root cause, #3/#4/#5 are the edit flow,
#6/#7/#8 touch the same `alarmclock.cpp` code paths.

**Files touched:**
- `alarmclock.yaml` (day_of_week offset in interval lambda)
- `alarmclock.cpp` (callbacks, sync_ui_, check_alarms_, load_from_storage_)
- `ui_clock_page.cpp` (day_of_week guard fix)

**Test:** Deploy and confirm alarms fire on the correct day. Exercise the
alarm edit flow end-to-end (tap row → time picker → change time/days/label → save).
Verify one-shot alarms appear after reboot. Test snooze-during-overlap scenario.

**DONE!!!**

---

### Batch 2 — UI layout & usability (P3): Issues #12–16, #20, #21

Rework display sizes and layout for bedside readability. These cascade (bigger
clock font shifts other elements, settings page overflow requires layout redo).

Issue #11 (larger clock font) deferred to Batch 3 — requires custom font
generation tooling.

**Files touched:**
- `ui_theme.h` (button sizes, Y offsets)
- `ui_clock_page.cpp` (larger font, reposition date/alarm labels)
- `ui_settings_page.cpp` (scrollable or condensed layout)
- `ui_time_picker.cpp` (cancel button, larger day buttons, keyboard)
- `ui_firing_overlay.cpp` (larger buttons/text)
- `ui_alarm_page.cpp` (fix add-button overlap)
- `ui.cpp` (swipe vs slider conflict)
- `alarmclock.yaml` `lvgl:` section (reference keyboard widget so it compiles in)

**Test:** Visually confirm all pages fit without overlap on 800×480. Test slider
dragging doesn't trigger page swipe. Verify time picker cancel/keyboard work.

**DONE!!!**

---

### Batch 3 — Display correctness & polish (P2 + P4): Issues #9, #10, #11, #17–19, #22–25

Safe, isolated improvements. Can be done incrementally.
Issue #11 (larger clock font) moved here from Batch 2.

**Files touched:**
- `alarmclock.h` (`format_next_alarm_text` — pass time format flag, fix `compute_content_color` comment/code)
- `ui_clock_page.cpp` (larger clock font, reposition date/alarm labels, call `ui_update_page_dots`)
- `ui_firing_overlay.cpp` (respect 24h format)
- `ui_alarm_page.cpp` (respect 24h format, "Once" label, firing highlight, larger fonts)
- `ui_clock_page.cpp` (call `ui_update_page_dots`)
- `ui.cpp` (call `ui_update_page_dots` from `ui_show_page`)
- `ui_settings_page.cpp` (larger slider knobs, pre-alarm option)
- `alarmclock.cpp` (skip volume ramp on re-fire)
- `storage.h` (add `pre_alarm_minutes` to StorageSettings)

**Test:** Toggle 12h/24h and confirm all time displays update. Verify page dots
track current page. Confirm firing alarm row is highlighted.

---

### Future (not in this plan)
- WAV/MP3 alarm sounds with custom partition table
- HA sync (restore alarms from HA entities on boot)
- Weather display on clock face
- Sunrise alarm (gradually increase brightness before alarm time)
