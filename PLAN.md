# Alarm Clock UI Implementation Plan

## Design Decisions (Finalized)

### Time Format
- Default to **12-hour**, but follow HA's setting if available at boot.
- User-configurable on-device via settings page.
- Also settable from HA.

### Alarm Sound
- **RTTTL for now** during development/testing.
- **Future:** WAV/MP3 files embedded in firmware. The ESP32-S3 has 16MB flash;
  firmware uses ~4-6MB, leaving ~10MB for data. A 10-second 16kHz mono WAV is
  ~320KB, so 20-30 alarm sounds can fit. Will need a custom partition table.

### Number of Alarms
- **4 max.**

### Alarm Labels
- Each alarm has a **label** (up to 15 characters, e.g., "Work", "Weekend").
- Shown in the alarm list row and on the firing overlay ("ALARM — Work").

### One-Shot Alarms
- If no days-of-week are selected, the alarm fires **once** at the set time
  and then **auto-disables** itself after firing.

### Snooze
- Default **9 minutes** (matches Lenovo Smart Clock / mechanical clock convention).
- **Global setting**, user-adjustable (5/9/10/15 min options).

### Alarm Firing Behavior
- Sound plays with **2-3 second pauses** between melody loops (like a real alarm).
- **Volume ramps up** gradually over ~30 seconds from low to configured level.
- **Auto-dismiss after 30 minutes** if no user interaction.

### Pre-Alarm Notification
- **5 minutes before** alarm time, show a **persistent banner** on the clock page.
- Banner shows alarm label and countdown: "Alarm in 5 min — Work", updating
  each minute ("4 min", "3 min", ...).
- **No sound** for the pre-notification.
- Banner transitions into the full firing overlay when the alarm fires.

### Persistence
- **Both NVS and HA.** NVS for offline reliability, sync with HA when connected.
- NVS stores: alarm configs (time, days, label, enabled), volume, brightness,
  snooze duration, time format preference.

### Screen Brightness / Sleep
- Normal: brightness = f(user_level, light_sensor) — existing logic.
- **Idle timeout (30 seconds):** dim to sensor-based minimum.
- **Tap to wake:** temporarily boost to user's configured level (still modulated
  by light sensor so it won't blind you in a dark room). Always applies (day
  and night) since in bright rooms the boost is imperceptible anyway.
- **Optional "full black" mode:** screen fully off when dark, tap to wake.
  Not default.

### Navigation
- **Swipe left/right** between Clock ↔ Alarms ↔ Settings pages.
- Firing overlay: **dedicated Snooze and Dismiss buttons** (no gesture dismiss).

### Deleting Alarms
- **Delete button** inside the alarm edit view.
- **Long-press** on alarm row shows delete confirmation.
- Both methods available.

### Next Alarm Indicator
- Show **both** on clock face: "7:00 AM (in 6h 30m)"

### Home Assistant Events
- `esphome.alarm_fired` — when alarm starts sounding.
- `esphome.alarm_dismissed` — when dismissed (from any source: screen, HA, auto).
- `esphome.alarm_snoozed` — when snoozed.
- These fire regardless of how the action was triggered.

### Home Assistant Entities
| Entity | Type | Purpose |
|---|---|---|
| `button.dismiss_alarm` | Button | Dismiss from HA / external button |
| `button.snooze_alarm` | Button | Snooze from HA / external button |
| `number.alarm_volume` | Number (0-100%) | Set volume from HA |
| `number.display_brightness` | Number (0-100%) | Set brightness from HA |
| `text_sensor.alarm_state` | Text Sensor | "idle" / "firing" / "snoozed" |
| `binary_sensor.alarm_firing` | Binary Sensor | True when firing |

---

## Implementation Tasks

### Task 1: Wire Audio Playback
**Files:** `alarmclock.cpp`, `alarmclock.yaml`, `__init__.py`
**What:** Call RTTTL when alarm fires. Implement looping with 2-3s pauses.
Implement volume ramp-up over ~30 seconds.
**Approach:** Pass RTTTL id to the component via config schema. Use a timer
to re-trigger playback after each `on_finished_playback` with a pause.
Volume ramp: start at 10% of configured volume, linearly increase each
loop iteration until reaching configured volume (~30s total).

---

### Task 2: Wire Light Sensor → Component
**Files:** `alarmclock.cpp`, `alarmclock.yaml`
**What:** Feed `sensor_factor_` from BH1750 readings into the component.
Add `set_sensor_factor(float)` method, call from sensor's `on_value` lambda.
**Straightforward, no design decisions needed.**

---

### Task 3: Time Picker UI
**Files:** New `ui_time_picker.cpp` (or extend `ui_alarm_page.cpp`)
**What:** Tap alarm row → show hour/minute rollers + day selector + label input.
**Widgets:** `lv_roller` for hour (1-12) and minute (00-59), AM/PM toggle,
7 toggle buttons for days, text area for label (up to 15 chars).
**Behavior:**
  - If no days selected → one-shot alarm (auto-disables after firing).
  - Confirm button saves, tapping outside cancels.
  - Delete button at bottom of edit view.
**Also:** Long-press on alarm row → delete confirmation dialog.

---

### Task 4: Alarm Sound Selector
**Files:** `ui_settings_page.cpp`
**What:** Replace static "Classic Beep" with dropdown of 4-5 RTTTL melodies.
**Future:** When WAV support is added, extend to show file-based sounds too.

---

### Task 5: NVS Persistence
**Files:** New `storage.h` / `storage.cpp`
**What:** Save/load to ESP32 NVS (esp-idf `nvs_flash` API).
**Data to persist per alarm:**
  - hour, minute, days_of_week, enabled, label[16]
**Global settings to persist:**
  - volume, brightness, snooze_duration, time_format_24h, selected_sound_index
**API:**
  - `void storage_save_alarm(uint8_t index, const AlarmTime &alarm)`
  - `bool storage_load_alarm(uint8_t index, AlarmTime *alarm)`
  - `void storage_save_settings(...)`
  - `void storage_load_settings(...)`

---

### Task 6: Screen Sleep / Wake on Tap
**Files:** `ui.cpp` or new `ui_sleep.cpp`, `alarmclock.cpp`
**What:**
  - After 30s idle: dim to sensor-based minimum.
  - Tap: boost to user's configured level (modulated by sensor).
  - Fade back after 30s idle again.
  - Optional "full black" mode (not default).
**LVGL:** Use `lv_disp_get_inactive_time()` for idle detection.

---

### Task 7: Snooze Duration Setting
**Files:** `ui_settings_page.cpp`, `alarm_state.h`
**What:** Roller or button group with 5/9/10/15 min options. Default 9 min.
Persist to NVS.

---

### Task 8: Page Transition Animations
**Files:** `ui.cpp` → `ui_show_page()`
**What:** Slide pages left/right using `lv_anim_t` instead of instant
show/hide. Lower priority — cosmetic only.

---

### Task 9: 12/24 Hour Toggle
**Files:** `ui_settings_page.cpp`, `ui_clock_page.cpp`, `alarmclock.cpp`
**What:**
  - Add toggle switch in settings page.
  - On boot, read HA's time format setting as default.
  - User can override on-device.
  - Also exposed as an HA entity for remote configuration.
  - Persist to NVS.

---

### Task 10: Pre-Alarm Banner
**Files:** New `ui_pre_alarm.cpp` or extend `ui_clock_page.cpp`
**What:** 5 minutes before alarm, show persistent banner on clock page.
  - Format: "Alarm in 5 min — Work"
  - Updates each minute: "4 min", "3 min", "2 min", "1 min"
  - Transitions into firing overlay when alarm fires.
  - No sound.

---

### Task 11: Auto-Dismiss Timer
**Files:** `alarm_state.h`, `alarmclock.cpp`
**What:** If alarm fires for 30 minutes with no interaction, auto-dismiss.
Fire `esphome.alarm_dismissed` event. Update state machine to track
firing duration.

---

### Task 12: HA Events
**Files:** `alarmclock.cpp`, `alarmclock.yaml`
**What:** Fire ESPHome events on state transitions:
  - `esphome.alarm_fired` — alarm starts sounding
  - `esphome.alarm_dismissed` — dismissed from any source
  - `esphome.alarm_snoozed` — snoozed from any source
**Approach:** Use `esphome::api::HomeAssistantServiceCallAction` or
the `homeassistant.event` action from the YAML side via a callback.

---

### Task 13: One-Shot Alarm Logic
**Files:** `alarm_time.h`, `alarmclock.cpp`
**What:** If `days_of_week == 0`, treat as one-shot. After firing + dismiss,
set `enabled = false`. Update NVS. Update UI alarm row.

---

### Task 14: Alarm Labels in AlarmTime
**Files:** `alarm_time.h`, `ui_alarm_page.cpp`, `ui_firing_overlay.cpp`
**What:** Add `char label[16]` to `AlarmTime`. Show in:
  - Alarm list row (below time)
  - Firing overlay header ("ALARM — Work")
  - Next-alarm indicator on clock face ("7:00 AM — Work (in 6h 30m)")

---

### Task 15: Next Alarm Display
**Files:** `ui_clock_page.cpp`, `alarmclock.cpp`
**What:** Compute nearest upcoming alarm, display on clock face as:
  "7:00 AM — Work (in 6h 30m)"
Update every minute. Use `minutes_until_alarm()` from `alarm_time.h`.

---

## Suggested Implementation Order

### Phase 1: Core Wiring (get the alarm actually working end-to-end)
1. ~~Task 14~~ — Add labels to AlarmTime (data model change, do first)
2. ~~Task 2~~ — Wire light sensor (easy, quick win)
3. ~~Task 1~~ — Wire audio playback (alarm can now make sound)
4. ~~Task 13~~ — One-shot alarm logic (small, data model)
5. ~~Task 11~~ — Auto-dismiss timer (small, state machine)
6. ~~Task 12~~ — HA events (small, wiring)

### Phase 2: Persistence (alarms survive reboots)
7. ~~Task 5~~ — NVS persistence

### Phase 3: UI (make it usable on-device)
8. **Task 3** — Time picker (biggest UI piece)
9. **Task 15** — Next alarm display on clock face
10. **Task 10** — Pre-alarm banner
11. **Task 4** — Alarm sound selector
12. **Task 7** — Snooze duration setting
13. **Task 9** — 12/24 hour toggle

### Phase 4: Polish
14. **Task 6** — Screen sleep / wake on tap
15. **Task 8** — Page transition animations
16. **Task 16** — Think about whether I need to prevent screen burn in

### Future (not in this plan)
- WAV/MP3 alarm sounds with custom partition table
- HA sync (restore alarms from HA entities on boot)
- Weather display on clock face
- Sunrise alarm (gradually increase brightness before alarm time)
