# Alarm Clock — Code Review & Fix Plan

## Review Summary

Full code review of alarm logic, state machine, scheduling, storage,
UI layout, and YAML configuration.  All 185 existing host-side tests pass.
Items below are grouped by severity and category.

---

## A. Logic / Correctness Bugs

### A1. Snooze / firing tick timer uses `millis()`, not real clock time
**Files:** `alarmclock.cpp` `loop()` (lines ~155-180)

`loop()` ticks `tick_snooze()` and `tick_firing()` every 60 000 ms using
`millis()`.  Meanwhile, `check_alarms_()` is driven by real clock time from
the 1-second YAML interval and deduplicates on actual minute changes
(`last_checked_minute_`).  These two clocks are independent and will drift
apart:

- `millis()` accumulates crystal error (~±20 ppm on ESP32-S3 = ~1.7 s/day).
- After an NTP time step or HA time sync jump the real minute boundary and
  the `millis()` boundary desynchronise.
- A 9-minute snooze could expire 9-10 minutes after it was set, or the
  auto-dismiss 30-minute counter could be off by a minute or more.

**Fix:** Replace the `millis()` timer in `loop()` with real-time minute
tracking.  Track `last_tick_minute_` (the real clock minute of the last
tick).  In `check_alarms_()`, when the minute changes, call `tick_snooze()`
and `tick_firing()` there instead of in `loop()`.  This keeps all
minute-level logic synchronised to the same clock source.

### A2. Long-press alarm delete has no confirmation dialog
**Files:** `ui_alarm_page.cpp` `alarm_row_longpress_cb()`

Long-pressing an alarm row calls `on_alarm_delete` directly, which resets
the alarm and hides the row with no confirmation.  The time picker's
"Delete" button correctly shows a confirmation dialog.  On a touch device
used bedside (drowsy, in the dark) accidental long presses are likely.

**Fix:** Route the long-press callback through the same delete-confirmation
overlay used by the time picker, or remove the long-press delete entirely
(the time picker already provides a confirmed delete path).

### A3. No validation of deserialized alarm hour/minute values
**Files:** `storage.h` `deserialize_alarm()`
**Could have been caught by tests:** YES — see D1.

After deserialising from NVS, `hour` and `minute` are used without range
checks.  Corrupted flash could load `hour=25` or `minute=99`, which would
never match any time and could overflow `snprintf` buffers in
`format_clock_time`.

**Fix:** After deserialising, clamp `hour` to 0–23, `minute` to 0–59, and
`days_of_week` to 0x7F.  Return false (or reset to defaults) if any field
is out of range.

### A4. `update_alarm_time()` silently auto-enables the alarm
**Files:** `alarmclock.cpp` `update_alarm_time()`

When the user edits an alarm's time via the time picker, the component
unconditionally sets `alarms_[index].enabled = true`.  If the user was
editing a *disabled* alarm just to change the time, the alarm becomes
enabled as a side-effect.

**Fix:** Remove the `alarms_[index].enabled = true` line.  The time picker's
confirm handler already calls `on_alarm_toggle` if the user checks the
enable state, and the save button should preserve whatever enable state was
set.  Alternatively, have the time picker explicitly pass the desired enable
state as part of the "save" callback.

### A5. Queued/pending alarm fires without re-checking time
**Files:** `alarmclock.cpp` `check_alarms_()` pending-alarm block

When an alarm is queued while another is firing (`pending_alarm_mask_`), the
queued alarm fires as soon as the current one is dismissed.  At that point,
only `alarms_[pending].enabled` is checked — the current hour/minute/day is
not re-validated.  This could fire the alarm minutes or hours after its
scheduled time.

**Fix:** Before firing a pending alarm, re-check that the alarm is still
relevant (e.g., within some grace period of its scheduled time, or that it
wasn't already snoozed/dismissed).  At minimum, warn in the log when firing
a stale pending alarm.

### A6. ~~Stale test file `tests/test_alarm.cpp` has incorrect assertions~~ RESOLVED

**Resolved:** Deleted `tests/test_alarm.cpp` — it was fully redundant with
`test_alarmclock.cpp` and contained a wrong assertion for one-shot alarms.

---

## B. UI Usability Issues

### B1. Firing overlay: Snooze and Dismiss buttons are same size with small gap
**Files:** `ui_firing_overlay.cpp`, `ui_theme.h`

Both buttons are 260×90 px with only a 20 px gap between them.  On a
bedside alarm clock, users are half-asleep and reaching blindly.
Accidentally hitting the wrong button is frustrating.

**Fix:**
- Make both buttons taller (e.g., 260×110 or 300×110) — easier to hit
  when groggy.  Keep them the same size (Dismiss is used frequently).
- Increase the gap between buttons to at least 40–50 px.
- Add brightness feedback to signal which button was pressed:
  - **Dismiss:** boost backlight to full brightness immediately (signals
    "you're getting up, here's light").
  - **Snooze:** dim backlight back to sleep-level (signals "going back to
    sleep").
  - This visual cue helps the user instantly realize if they hit the wrong
    button and correct before fully waking or falling back asleep.

### B2. Firing overlay uses 48 px font for time instead of the 120 px clock font
**Files:** `ui_firing_overlay.cpp`

The clock page uses the custom `clock_font_120` for the time, but the
firing overlay drops to `montserrat_48`.  When the user wakes to an alarm,
the time should be *more* visible, not less.

**Fix:** Use `clock_font_120` (or at least a larger font like
`montserrat_48` → `montserrat_48` is OK if 120 would crowd the buttons).
Rebalance the firing overlay layout to accommodate a larger time label,
potentially by moving the "ALARM" text into a smaller top banner and
shifting buttons further down.

### B3. Alarm toggle switches are too small (50×26 px)
**Files:** `ui_alarm_page.cpp`

On a 4.3″ 800×480 display (~217 PPI), 50×26 px ≈ 6×3 mm.  This is
difficult to tap accurately, especially in low light or while drowsy.
LVGL's default switch size is similarly small.

**Fix:** Increase switch size to at least 60×32 or 70×36 px.  LVGL switches
scale with `lv_obj_set_size()`.

### B4. Slider knob touch targets are small
**Files:** `ui_settings_page.cpp`

Volume and brightness sliders have `pad_all = 8` on the knob part, giving a
touch target of roughly 16+knob_size px.  Difficult to grab and drag on a
touch panel.

**Fix:** Increase knob padding to at least 12–14 px, or increase the slider
height (currently defaults to ~10 px track).  Consider making the slider
track taller (e.g., 20 px) as well.

### B5. Page indicator dots are too small (8 px) and poorly visible
**Files:** `ui_clock_page.cpp` `ui_build_page_dots()`

8 px dots at the bottom of the screen are hard to see in a dark room and
don't clearly communicate that swiping is possible.

**Fix:** Increase dot size to 12–14 px.  Consider adding a subtle swipe hint
on first boot or when the user taps the dot area.

### B6. Settings page content extends below screen, not obvious it scrolls
**Files:** `ui_settings_page.cpp`

The pre-alarm roller starts at Y=530 on a 480 px screen.  The page is
scrollable, but there is no visual cue (no scroll indicator, no fade/shadow
at the bottom edge) that more content exists.

**Fix:**
- Add an LVGL scroll indicator bar (`lv_obj_set_scrollbar_mode(...,
  LV_SCROLLBAR_MODE_AUTO)`).
- Alternatively, reorganise settings into a more compact layout (e.g.,
  two-column layout for the switch/roller sections) so everything fits
  without scrolling.

### B7. Settings page scrolling conflicts with swipe page navigation
**Files:** `ui.cpp` `swipe_event_cb()`, `ui_settings_page.cpp`

Horizontal swipes on the scrollable settings page trigger page transitions.
A user trying to interact with a roller (which involves horizontal-ish
finger movement) could accidentally switch pages.

**Fix:** Disable swipe detection on the settings page, or increase
`kSwipeThreshold` for the settings page.  Add explicit "back" / page
navigation buttons instead of relying solely on swipe.

### B8. Time picker keyboard covers lower portion of picker panel
**Files:** `ui_time_picker.cpp`

The on-screen keyboard is 240 px tall (half the screen) and positioned at
the bottom.  When shown, it covers the Confirm/Cancel/Delete buttons and
potentially the day-of-week buttons.  The user can't save without first
dismissing the keyboard.

**Fix:**
- When the keyboard appears, scroll/reposition the picker panel upward so
  buttons remain accessible.
- Or, move the label input (and keyboard trigger) to a sub-overlay/dialog
  so the main picker stays unobstructed.
- Add dismiss-on-tap-outside for the keyboard.

### B9. No way to dismiss on-screen keyboard by tapping outside
**Files:** `ui_time_picker.cpp`

The keyboard only hides on `LV_EVENT_READY` (checkmark) or
`LV_EVENT_CANCEL`.  Tapping outside the keyboard does nothing, which is
unintuitive for touch users.

**Fix:** Add an `LV_EVENT_DEFOCUSED` handler on `label_input_` (or a click
handler on the picker panel) that hides the keyboard.

### B10. No alarm sound preview
**Files:** `ui_settings_page.cpp`

Users can select an alarm sound from the roller but have no way to hear it
until the alarm actually fires.

**Fix:** Add a "Preview" / play button next to the sound roller.  When
tapped, play a short snippet of the selected RTTTL melody via the existing
RTTTL component.

### B11. "Add Alarm" button may overlap page indicator dots
**Files:** `ui_alarm_page.cpp`, `ui_clock_page.cpp`

The "Add Alarm" button bottom edge is at Y=460 (aligned `BOTTOM_MID, 0,
-20`).  Page dots are screen-root children at Y=465 (`BOTTOM_MID, ...,
-15`).  They likely overlap by a few pixels.

**Fix:** Move the "Add Alarm" button higher (e.g., `-40` instead of `-20`)
or move page dots further down (`-8`).

### B12. Alarm row labels are tightly packed — hard to read in low light
**Files:** `ui_alarm_page.cpp`

Within a 92 px tall alarm row, the time label (28 px font at Y=-18), alarm
label (18 px font at Y=+12), and days label (18 px font at Y=+30) are
stacked with only ~12 px visual gaps.  In low light on a bedside clock this
makes the row hard to scan quickly.

**Fix:** Either increase the row height to ~110 px to give more breathing
room, or reduce to two lines (time + days on one line, label on the other).

---

## C. Minor / Improvement Items

### C1. ~~`test_alarm.cpp` is orphaned — not compiled by CI~~ RESOLVED

**Resolved:** Same issue as A6.  File deleted.

### C2. Settings page uses fragile absolute Y positioning
**Files:** `ui_settings_page.cpp`

All settings widgets use hard-coded Y coordinates.  Adding or removing a
widget requires manually recalculating every subsequent Y value.

**Fix:** Use LVGL flex layout (`lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN)`)
or a single-column container with automatic spacing.

### C3. No explicit page navigation buttons / affordances
**Files:** `ui.cpp`, all page files

Navigation is entirely swipe-based.  New users, or users waking up groggy,
might not discover other pages exist.

**Fix:** Add small left/right arrow tap zones at the screen edges, or a
brief tutorial overlay on first boot.

### C4. Copilot-instructions.md references `rpi_dpi_rgb` but YAML uses `mipi_rgb`
**Files:** `.github/copilot-instructions.md`, `alarmclock.yaml`

The display platform name changed in newer ESPHome versions.

**Fix:** Update copilot-instructions.md to say `mipi_rgb` to match the
YAML.

### C5. No haptic/visual press feedback on firing overlay buttons
**Files:** `ui_firing_overlay.cpp`

No explicit pressed-state styling is set on the snooze/dismiss buttons.
LVGL has a default pressed effect, but a more obvious color shift or brief
animation would help confirm the press registered in a dark room.

**Fix:** Add `LV_STATE_PRESSED` style overrides with a visible brightness or
color shift.

### C6. Settings rollers show only 2 visible rows — harder to browse
**Files:** `ui_settings_page.cpp`

The sound roller has 5 options but shows only 2 rows.  This makes it hard
to see what options are available.

**Fix:** Increase `lv_roller_set_visible_row_count()` to 3 for the sound
roller (the snooze and pre-alarm rollers with 4 options are fine at 2–3).

---

## D. Missing Tests

The following tests should be added to `tests/test_alarmclock.cpp` to
prevent regressions on the bugs found above.  All are pure-function tests
that compile on the host under `-DUNIT_TEST`.

### D1. `deserialize_alarm` must reject or clamp out-of-range fields (prevents A3)

The existing tests cover round-trips and version mismatches, but never
feed invalid *field* values.  `deserialize_alarm` is a pure function
already under test — these are trivial to add.

```cpp
TEST(deserialize_alarm_invalid_hour) {
    // Manually craft a buffer with hour=25 (out of range 0-23).
    AlarmTime orig{12, 0, kWeekdays, true};
    uint8_t buf[kSerializedAlarmSize];
    serialize_alarm(orig, buf, sizeof(buf));
    buf[1] = 25;  // corrupt hour field
    AlarmTime loaded{};
    // After fix: should return false or clamp hour to valid range.
    ASSERT_FALSE(deserialize_alarm(buf, sizeof(buf), &loaded));
    PASS();
}

TEST(deserialize_alarm_invalid_minute) {
    AlarmTime orig{12, 0, kWeekdays, true};
    uint8_t buf[kSerializedAlarmSize];
    serialize_alarm(orig, buf, sizeof(buf));
    buf[2] = 70;  // corrupt minute field
    AlarmTime loaded{};
    ASSERT_FALSE(deserialize_alarm(buf, sizeof(buf), &loaded));
    PASS();
}

TEST(deserialize_alarm_invalid_days) {
    AlarmTime orig{12, 0, kWeekdays, true};
    uint8_t buf[kSerializedAlarmSize];
    serialize_alarm(orig, buf, sizeof(buf));
    buf[3] = 0xFF;  // bit 7 set — only bits 0-6 are valid days
    AlarmTime loaded{};
    // After fix: should mask to 0x7F or reject.
    bool ok = deserialize_alarm(buf, sizeof(buf), &loaded);
    if (ok) {
        ASSERT_EQ(loaded.days_of_week & 0x80, 0);
    }
    PASS();
}
```

### D2. `format_clock_time` with out-of-range hour/minute (prevents A3 downstream)

If corrupted data reaches display code, the output is wrong but doesn't
crash.  Tests documenting the expected behaviour (either garbage output
or a guarded fallback) pin down the contract.

```cpp
TEST(format_clock_time_invalid_hour_24h) {
    char buf[16];
    // hour=25 in 24h mode → currently produces "25:00"
    // After fix: should produce a safe fallback like "--:--" or clamp.
    size_t n = format_clock_time(25, 0, true, buf, sizeof(buf));
    // Document whatever the fixed contract is.
    ASSERT_TRUE(n > 0);
    PASS();
}

TEST(format_clock_time_invalid_minute) {
    char buf[16];
    // minute=70 → currently produces "07:70" (24h) or "7:70 AM" (12h)
    size_t n = format_clock_time(7, 70, true, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    PASS();
}
```

### D3. `deserialize_settings` with out-of-range field values

Similar to D1, but for `StorageSettings`.  Volume and brightness are
floats that should be clamped to [0.0, 1.0], `selected_sound_index`
should be < `kAlarmSoundCount`, and `snooze_duration_minutes` should be
a valid option.

```cpp
TEST(deserialize_settings_invalid_volume) {
    StorageSettings orig{};
    orig.volume = 0.5f;
    uint8_t buf[kSerializedSettingsSize];
    serialize_settings(orig, buf, sizeof(buf));
    // Corrupt volume to a NaN or >1.0 value.
    float bad = 2.0f;
    memcpy(&buf[1], &bad, sizeof(float));
    StorageSettings loaded{};
    bool ok = deserialize_settings(buf, sizeof(buf), &loaded);
    if (ok) {
        ASSERT_TRUE(loaded.volume >= 0.0f && loaded.volume <= 1.0f);
    }
    PASS();
}

TEST(deserialize_settings_invalid_sound_index) {
    StorageSettings orig{};
    uint8_t buf[kSerializedSettingsSize];
    serialize_settings(orig, buf, sizeof(buf));
    buf[10] = 99;  // invalid sound index
    StorageSettings loaded{};
    bool ok = deserialize_settings(buf, sizeof(buf), &loaded);
    if (ok) {
        ASSERT_TRUE(loaded.selected_sound_index < kAlarmSoundCount);
    }
    PASS();
}
```

### D4. ~~CI should compile `test_alarm.cpp`~~ RESOLVED

**Resolved:** File deleted (see A6).  No CI change needed.

### D5. `hour_24_to_12` / `hour_12_to_24` with out-of-range input

These are pure functions used in the time picker confirm path.  If a
corrupted alarm time reaches them, the output is unpredictable.

```cpp
TEST(hour_24_to_12_out_of_range) {
    // hour_24=25 → 25%12=1, is_pm=(25>=12)=true → {1, true}
    // This is "wrong" but safe.  Document the behaviour.
    auto [h, pm] = hour_24_to_12(25);
    // After fix: should clamp or assert.
    ASSERT_TRUE(h >= 1 && h <= 12);
    PASS();
}

TEST(hour_12_to_24_out_of_range) {
    // hour_12=0 → should never happen (valid range 1-12).
    // Currently: is_pm?12:0 path not taken, returns 0+12=12 or 0.
    uint8_t h = hour_12_to_24(0, false);
    // Document: 0 AM → 0 (midnight). Happens to be "correct" but
    // the input is invalid.
    ASSERT_TRUE(h < 24);
    PASS();
}
```

### Summary: testability of each issue

| Issue | Testable? | Why / why not |
|-------|-----------|---------------|
| A1    | No        | Architectural: bug is *when* tick is called (millis vs real time), not *what* tick does. tick functions themselves are well-tested. |
| A2    | No        | UI behaviour (LVGL event routing). |
| A3    | **Yes**   | `deserialize_alarm` is a pure function already under test. Missing invalid-input edge cases. → D1, D3 |
| A4    | No        | Component method (`update_alarm_time`), not a pure function testable on host. |
| A5    | No        | Component method (`check_alarms_`), inline in ESPHome-dependent code. |
| A6    | **Yes**   | The stale test *is* the test issue. CI should compile it. → D4 |
| B1–12 | No        | UI layout/sizing — requires visual inspection or UI automation. |
| C1–6  | No        | Config, docs, or UI structure. |

---

### Future (not in this plan)
- WAV/MP3 alarm sounds with custom partition table
- HA sync (restore alarms from HA entities on boot)
- Weather display on clock face
- Sunrise alarm (gradually increase brightness before alarm time)
