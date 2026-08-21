# Display flicker, scroll performance, and minimum-brightness notes

Working notes captured while fixing the UI bugs, so a follow-up session can pick
up the display/backlight issues without re-deriving everything.

## Hardware / config facts

| Item | Value |
|---|---|
| Board | Elecrow CrowPanel Advance 4.3" — one unit is v1.1, a second (newer) unit is v1.3 |
| Panel | `mipi_rgb`, model `RPI`, 800×480, `pclk_frequency: 14MHz`, `color_order: RGB` |
| Porches | hsync 8/4/43 (front/pulse/back), vsync 8/4/12 |
| PSRAM | octal, 80 MHz |
| Framework | `esp-idf` |
| LVGL | ESPHome `lvgl:` component, `buffer_size: 12%`, no `full_refresh` override |
| Backlight | I²C byte written to STC8H1K28 @ `0x30`: `0` = brightest, `244` = dimmest, `245` = off |
| Light sensor | BH1750 @ `0x23`, `update_interval: 5s` — **confirmed working** |

Relevant code:

- `update_backlight_()` — [components/alarmclock/alarmclock.cpp](../components/alarmclock/alarmclock.cpp#L606)
- `brightness_to_pwm()` — [components/alarmclock/alarmclock.h](../components/alarmclock/alarmclock.h#L272)
- `set_sensor_factor()` (deadband 0.05, EMA alpha 0.2) — [components/alarmclock/alarmclock.cpp](../components/alarmclock/alarmclock.cpp#L397)
- `lux_to_sensor_factor()` (maps 0.5–15 lx → 0.0–1.0) — [components/alarmclock/alarmclock.h](../components/alarmclock/alarmclock.h#L193)
- Clock tick / redraw throttling — [alarmclock.yaml](../alarmclock.yaml#L218)

## Observed symptoms

### Bug 4 — intermittent flicker

- Happens while completely idle, no touch input.
- Seen on both the clock page and the settings page.
- Roughly once a minute or more; **not** reliably tied to the minute rollover.
- Seen in peripheral vision: not a band of garbage; possibly a tear line, possibly
  a whole-screen brightness blip. Not conclusively characterized.

### Bug 2 (residual) — sluggish scrolling

- Settings page content lags behind the finger and moves in coarse jumps.
- Elecrow's stock v1.3 LVGL demo firmware on the same hardware feels noticeably
  more responsive, so this is very likely a render-pipeline configuration issue,
  not an LVGL widget-tree issue. (Momentum, elastic scroll, and scroll animations
  are already disabled; `SCROLL_ON_FOCUS` was cleared on the buttons.)

### Bug 5 — minimum brightness too high

- BH1750 verified working — covering the sensor hole gives `0.1 lx`, uncovering gives `8.1 lx`:

```
[17:21:46.545][S][sensor]: 'Ambient Light' >> 0.1 lx
[17:21:51.546][S][sensor]: 'Ambient Light' >> 0.1 lx
[17:21:56.549][S][sensor]: 'Ambient Light' >> 0.1 lx
[17:22:01.544][S][sensor]: 'Ambient Light' >> 8.1 lx
```

**Important deduction:** at slider 0%, `brightness_ = 0`, so `update_backlight_()`
computes `bright = 0` and `brightness_to_pwm()` returns `244` — the dimmest value
the backlight controller accepts (`245` is off). The hardware minimum backlight
is already in use, so backlight math alone cannot make the display darker.

1. Verify the actual command range the STC8H1K28 accepts (Elecrow's v1.3 docs /
   Arduino examples) — the 0–244/245 mapping came from a v1.1-era example and may
   be wrong or coarser than the real range.
2. Below 25% slider brightness, an inherited LVGL color filter now shades all UI
   colors smoothly toward black. At 0%, white content is approximately
   `0x404040`. This neutral-grey approach preserves color meaning and avoids
   maintaining separate night colors for every widget. The alarm-firing overlay
   opts out so Snooze and Dismiss controls remain fully colored and legible.

The filter changes draw colors directly rather than lowering whole-object
opacity, so it does not require a full-screen intermediate compositing layer.

## Hypotheses to test for the flicker (ranked)

1. **Backlight PWM stepping from the ambient sensor.** `set_sensor_factor()` runs
   every 5 s; its deadband compares the *raw* new factor against the *smoothed*
   stored factor, so a persistent gap keeps nudging the PWM value every 5 s. Each
   nudge is a real backlight step and could read as a "flicker". Test: temporarily
   comment out the BH1750 `on_value` action (or hard-code `sensor_factor_ = 1.0f`)
   and watch for an hour.
2. **RGB panel DMA underrun / PSRAM bandwidth contention.** Classic ESP32-S3 RGB
   symptom — a tear line or momentary glitch when Wi-Fi/API traffic or a flash
   cache disable (NVS write) starves the LCD DMA. ESPHome's `mipi_rgb` does not
   expose a bounce-buffer option, so levers are: lower `pclk_frequency`
   (14 MHz → 12 → 10), adjust porches, reduce Wi-Fi/API chatter (the template
   `number` entities publish every 5 s and `Alarm State` publishes every second —
   that once-per-second publish is a suspicious match for the observed cadence).
3. **LVGL draw buffer placement/size.** The current test configuration explicitly
   uses `buffer_size: 12%`. ESPHome recommends this size on PSRAM-equipped devices
   because the approximately 92 KB RGB565 buffer can be allocated in faster
   internal RAM when space permits. Check startup logs for allocation failures and
   compare scrolling and flicker against the previous default allocation behavior.
   An explicit size disables ESPHome's runtime fallback, so successful startup is
   part of the hardware acceptance test.

Note that hypothesis 3 could explain bugs 2 and 4 together, and matches the
observation that Elecrow's stock demo is much smoother. The 12% buffer is now the
baseline under test rather than a proposed experiment.

## What was already fixed (do not redo)

- Clock label now re-renders immediately when the 12/24 h setting changes
  (`ui_refresh_clock()` in `ui_clock_page.cpp`, called from `set_time_format_24h()`).
- Settings page now has a fixed header (title + home button) with a separate
  scrollable body, mirroring the alarm page.
- `LV_OBJ_FLAG_SCROLL_ON_FOCUS` cleared on settings buttons so pressing a button
  near the edge no longer shifts the page under the finger.

---

## Prompt for the follow-up session

> I'm working on the `esphome-alarmclock` repo (Elecrow CrowPanel Advance 4.3",
> ESP32-S3, ESPHome `esp-idf` + LVGL, `mipi_rgb` display). Please read
> `docs/display-tuning-notes.md` first — it has the hardware facts, the code
> pointers, the symptoms, and the ranked hypotheses.
>
> I need help with three related hardware/render issues:
>
> 1. **Intermittent flicker while the screen is idle** (both the clock page and
>    the settings page, roughly once a minute or more, not tied to the minute
>    rollover, possibly a tear line).
> 2. **Sluggish scrolling** on the settings page — content lags behind my finger
>    and moves in coarse jumps. Elecrow's stock LVGL demo firmware on the same
>    board is much smoother, so I believe the render pipeline is misconfigured.
> 3. **Minimum brightness is too high** — at 0% on the brightness slider the
>    display is still too bright for a dark bedroom. Note the backlight controller
>    is already being driven at its dimmest accepted value (244; 245 = off), so
>    this probably needs either a corrected backlight command range or a dimmed
>    color palette for night use.
>
> Please start by validating hypothesis 3 in the notes (the configured 12% LVGL
> draw buffer and its placement in internal RAM vs PSRAM, plus `mipi_rgb` timing),
> since it could explain both 1 and 2. Tell me what to flash and what to look for
> at each step; I can run the hardware and report back.
> Ask me questions before making changes if anything is ambiguous, and keep
> protocol/hardware changes conservative.
