# Alarm Clock — Code Review & Fix Plan

## LVGL Responsiveness and Alarm Sound Picker

The LVGL widgets demo relies on native scrolling and tabview behavior, while
the alarm clock currently receives new GT911 coordinates every 50 ms and does
additional work from control callbacks. Make the changes below as separate PRs
so firmware compilation runs in CI and each hardware result remains attributable
to one change.

### PR 1 — Increase touchscreen sample rate

- Set the GT911 `update_interval` to 16 ms, increasing input sampling from about
	20 Hz to about 60 Hz without changing display timing or hardware pins.
- Compare finger tracking on both Settings and Alarms with the current firmware.
- Confirm that touch remains reliable while the BH1750 shares the I2C bus and
	watch logs for I2C warnings.
- Keep the change only if scrolling becomes smoother without introducing missed
	touches, I2C errors, or display instability.

**Hardware result:** Merged and tested. No meaningful responsiveness change was
visible, but no adverse touch, display, or I2C behavior was observed. Retain the
16 ms interval, but treat the original 50 ms polling rate as disproven as the
primary bottleneck.

### PR 2 — Reduce work during slider drags

- Keep `LV_EVENT_VALUE_CHANGED` local to the UI: update only the displayed
	percentage while the finger is moving.
- Commit volume and brightness changes through `AlarmClockComponent` on
	`LV_EVENT_RELEASED`; preserve vertical-scroll cancellation so scrolling over a
	slider does not save an accidental value.
- Avoid duplicate slider/label updates and repeated `INFO` logs during a drag.
- Verify that Home Assistant values and persisted settings receive the final
	released value, and that brightness applies promptly when the slider is
	released.

### PR 3 — Restore native scroll behavior

- Re-enable LVGL scroll momentum on Settings, matching the widgets demo and the
	existing Alarms page behavior.
- Evaluate the custom `LV_EVENT_PRESSING` gesture handlers after the higher touch
	sample rate is in place. Remove only handlers that duplicate LVGL's native
	press-lost and scroll-chain behavior without protecting a real interaction.
- Compare short drags, fast flicks, scrolling that starts over a control, and
	accidental row activation.

### PR 4 — Replace the alarm sound roller with a dropdown

- Enable the LVGL dropdown widget in the YAML bootstrap page so ESPHome includes
	it in the generated build.
- Replace `sound_roller_` with a full-width `lv_dropdown` labeled `Alarm Sound`,
	using the existing `kAlarmSounds` names and persisted sound index.
- On `LV_EVENT_VALUE_CHANGED`, commit the selected sound and request one debounced
	preview. Do not preview merely when the menu opens or closes.
- Keep the dropdown inside the vertical settings scroller, disable automatic
	scroll-on-focus, and verify that opening its popup does not move the page.
- Verify selection, preview, persistence after reboot, popup scrolling, and
	cancellation when leaving Settings.

### Possible follow-up — Native page swiping

After vertical scrolling is responsive, prototype a hidden-tab-bar `lv_tabview`
or horizontal snap container for `Settings | Clock | Alarms`. Keep this separate
from the fixes above because it changes the navigation model and requires direct
hardware evaluation of horizontal-versus-vertical gesture arbitration.

---

## UI Navigation Refresh

Navigation now uses explicit `Alarms`, `Settings`, and `Home` buttons rather
than swipe gestures. Alarm and Settings pages share one fixed-header helper,
so title and Home-button layout, styling, and touch behavior remain consistent.

Revisit page organization only if Settings grows enough to justify sections.

---

### Future (not in this plan)
- WAV/MP3 alarm sounds with custom partition table
- HA sync (restore alarms from HA entities on boot)
- Weather display on clock face
- Sunrise alarm (gradually increase brightness before alarm time)
---

## Brightness / Screen Dim-on-Wake Resolution

The sleep and awake paths previously used different formulas. In a dark room,
the fixed sleep dim amount could produce a higher brightness than the
ambient-scaled awake calculation, causing a tap to make the display dimmer.

Both paths now use one tested calculation. Sleep brightness retains the
day/night dim behavior but is capped at 80% of the corresponding awake level,
so waking the display cannot reduce its brightness.
