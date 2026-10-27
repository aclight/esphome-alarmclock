# esphome-alarmclock

Standalone bedside alarm clock for the **Elecrow CrowPanel Advance 4.3"**
(ESP32-S3) and **CrowPanel Advance 5.0"** (ESP32-P4). Both use an 800×480
capacitive touch IPS display running **ESPHome** with an **LVGL** touchscreen UI.

## Hardware

### Boards

| Entry file | Board | Processor | Connectivity |
|---|---|---|---|
| `alarmclock.yaml` | CrowPanel Advance 4.3" v1.1 | ESP32-S3, 16 MB flash, 8 MB PSRAM | Native Wi-Fi |
| `alarmclock-p4-5inch.yaml` | CrowPanel Advance 5.0" | ESP32-P4, 16 MB flash, 32 MB PSRAM | ESP32-C6 over ESP-Hosted SDIO |

The 5-inch configuration renders the existing 800×480 interface without UI
scaling. Board-specific display, touch, audio, and controller settings are
overridden in its entry file while the application configuration remains shared.

### 4.3-inch Pin Map

| Function | GPIO | Notes |
|---|---|---|
| I2C SDA | IO15 | Touch (GT911 @ 0x5D), BH1750 (@ 0x23) |
| I2C SCL | IO16 | Shared bus |
| I2S DOUT | IO4 | MAX98357A data |
| LCD backlight | IO2 | PWM-dimmable via LEDC (V1.0); I2C via STC8H1K28 @ 0x30 (V1.1/V1.2) |
| LCD DE | IO42 | Display data enable |
| LCD VSYNC | IO41 | Display vertical sync |
| LCD HSYNC | IO40 | Display horizontal sync |
| LCD PCLK | IO39 | Display pixel clock |
| LCD D0–D15 | Various | See `alarmclock.yaml` for full list |

### DIP Switch

The CrowPanel Advance 4.3" has a 4-position DIP switch used during flashing and boot mode selection. **All switches should be OFF during normal operation.**

### Peripherals

| Peripheral | Interface | Address / Detail |
|---|---|---|
| GT911 touch controller | I2C | 0x5D |
| BH1750 light sensor | I2C | 0x23 (optional, external) |
| MAX98357A I2S amplifier | I2S | On-board, output via SPK connector |

## Features Plan

- **Clock display** — Large LVGL clock face, auto-dimming based on ambient light
- **Alarm management** — Multiple alarms with day-of-week scheduling, snooze/dismiss via touch
- **Audio playback** — Alarm tones via I2S → MAX98357A → speaker
- **Adaptive display brightness** — Calibrated ambient-light control with separate daytime and night preferences
- **Home Assistant integration** — Alarm control, sensor data, OTA updates via ESPHome API
- **LVGL touchscreen UI** — Clock faces, alarm configuration, settings screens
- **Optional Home Assistant controls page** — Up to 6 toggle, pushbutton and dimmer controls
- **Optional minisplit page** — Power, target temperature, HVAC mode, fan speed and vane controls for a Home Assistant climate entity

## Optional Pages

Both pages are off unless configured, and each can be enabled independently.
When enabled, a lightbulb (controls) and/or snowflake (minisplit) button
appears in the top bar of the clock page. Each page has a Home button and shows
the current time in its header. Both need Home Assistant 2026.10 or newer with
`homeassistant_states: true` and `homeassistant_services: true` under `api:`
(already set in `alarmclock.yaml`). While the API is disconnected, or an
entity is `unavailable`/`unknown`, the UI shows it as unavailable and ignores
presses on it.

Home Assistant also has to allow the clock to call actions: go to
**Settings > Devices & services > ESPHome**, choose **Configure** on the alarm
clock and enable **Allow the device to perform Home Assistant actions**. New
ESPHome devices have this off. Without it, buttons show the right state, but
pressing them does nothing and Home Assistant raises a repair issue. Each call
is logged on the device as `Calling Home Assistant action ...`.

### Controls page

```yaml
alarmclock:
  controls_page:
    controls:            # up to 6, shown in a 3x2 grid
      - type: toggle     # tap toggles the entity and mirrors its state
        name: Desk Lamp
        entity_id: light.desk_lamp
      - type: dimmer     # slider sets brightness; releases call light.turn_on
        name: Bedside
        entity_id: light.bedside
      - type: action     # pushbutton that runs ESPHome actions
        name: Goodnight
        on_press:
          - homeassistant.action:
              action: automation.trigger
              data:
                entity_id: automation.goodnight
```

`toggle` calls `homeassistant.toggle` on any entity that supports it.
`dimmer` expects a `light` entity and maps Home Assistant's 0-255 `brightness`
attribute to 0-100%.

Each card shows an icon picked from the entity's domain (a lightbulb for
`light`, a plug for `switch`, a fan for `fan`, and a power symbol for anything
else), filled when on and outlined when off. Toggle cards also show an on/off
switch, and dimmer cards show the brightness slider.

### Minisplit page

```yaml
alarmclock:
  minisplit_page:
    climate_entity_id: climate.master_bedroom_climate
    horizontal_vane_entity_id: select.mitsubishi_heat_pump_horizontal_vane
    vertical_vane_entity_id: select.mitsubishi_heat_pump_vertical_vane
    # Optional overrides (defaults shown)
    hvac_modes: [off, heat, cool, heat_cool, dry, fan_only]
    fan_modes: [auto, quiet, low, medium, high]
    min_temperature: 60
    max_temperature: 88
    temperature_step: 1
    temperature_unit: "°F"
```

The page ports the behaviour of `m5stack-dial-thermostat.yaml`: a power
button (`climate.toggle`), +/- target temperature (clamped to the min/max),
mode and fan pickers, and previous/next vane buttons (`select.select_next` /
`select.select_previous` with `cycle`). The power button sits on its own in
the bottom-left corner so it is hard to press by accident, and the mode and
fan pickers highlight the current selection. Set `hvac_modes` and `fan_modes` to
the values your climate entity actually supports (up to 8 each).

## Software Stack

- **ESPHome** (`esp-idf` framework) — core firmware, OTA, Home Assistant API
- **LVGL** — touchscreen UI rendering
- **Custom C++ component** (`components/alarmclock/`) — alarm logic, backlight control, audio

## Adding WAV Alarm Sounds

Place a 16-bit PCM WAV file in the repository, then convert it to the embedded
16 kHz mono format with the included script. The converter also accepts stereo
or other sample rates and downmixes/resamples them:

```bash
python scripts/wav_to_pcm.py path/to/source.wav components/alarmclock/my_sound_pcm.h --symbol kMySoundPcm
```

Include the generated header from `components/alarmclock/alarmclock.h`, add a
`SoundKind::kSample` entry to `kAlarmSounds[]` using the generated symbol and
`sizeof`, and increase both `kAlarmSoundCount` and
`kMaxStoredSoundIndex` in `storage.h` together. The generated PCM header is
embedded in flash, so keep clips short.
Add the generated PCM header file to Git as well.

## Assembly Notes

1. The CrowPanel comes pre-assembled; no soldering required.
2. Power via USB-C (5 V). The board has an on-board voltage regulator.
3. Speaker connects to the on-board SPK connector (2-pin, wired to the MAX98357A).

### BH1750 Light Sensor Wiring

The BH1750 is an optional external module that connects to the CrowPanel's I2C header.

| BH1750 Pin | Connect to | Note |
|---|---|---|
| VCC | 3.3V | From CrowPanel I2C header |
| GND | GND | Any ground pin |
| SDA | IO15 | Shared I2C data line |
| SCL | IO16 | Shared I2C clock line |
| ADDR | GND | Sets I2C address to 0x23 |

The CrowPanel Advance 4.3" exposes a 4-pin I2C connector (IO15/IO16/3.3V/GND). If your BH1750 breakout board has a matching connector (e.g. STEMMA QT / Qwiic), you can plug it in directly. Otherwise, use jumper wires to the header pins.

My BH1750 board uses I2C address 0x23 if ADDR is floating or grounded and 0x5C if it is tied to VCC.

Brightness adapts to the room like a phone's adaptive brightness. Moving the
Display Brightness slider (on the clock or in Home Assistant) teaches the clock
what you want in the light you are in, and it follows the room from there. The
learned curve survives reboots; the Reset Learned Brightness button clears it.
Minimum Brightness (on the clock and in Home Assistant) and Maximum Brightness
(Home Assistant configuration) bound the result. Until you first adjust it, the
curve rises from the minimum in the dark to the maximum at the Full Brightness
Ambient Light level (Home Assistant configuration). The firmware also applies
neutral content shading when the backlight reaches the bottom of its useful
range. The large time uses solid numbers at every brightness, including at night.
Without a light sensor the slider is plain manual brightness. The curve
comes from the
[esphome-shared-components](https://github.com/aclight/esphome-shared-components)
`adaptive_brightness` component.

In Home Assistant, the controls (Alarm Volume, Display Brightness, Minimum
Brightness) are the settings the clock's own Settings page also changes. The
configuration entities (Maximum Brightness, Full Brightness Ambient Light,
Reset Learned Brightness) only tune the adaptive curve.

The Next Alarm sensor reports when the next alarm will fire as a timestamp, and
is unknown when none is scheduled. It accounts for a pending skip.

## RGB Panel Tuning (ESP32-P4)

The panel has no frame memory of its own, so the ESP32 clocks out every pixel in
real time from a pair of small bounce buffers kept fed by DMA. Two options in
`alarmclock-p4-5inch.yaml` are needed to make that reliable, and they fix two
independent defects — both are required, neither alone is sufficient:

- `bounce_buffer_lines: 40` — scanlines per bounce buffer. Upstream ESPHome
  hardcodes 10, which starves the DMA and tears constantly. Must divide the
  display height. Costs 2 × 800 × 40 × 2 = 128 KB of internal SRAM.
- `force_restart: false` — upstream otherwise resets the FIFO and GDMA on every
  VSYNC, but the interrupt handler has only the 4-line vertical back porch
  (~182 µs at 18 MHz) to finish. Every late frame measured missed that deadline,
  so the restart re-sent pixels that were already on the wire, giving a
  shifted/wrapped image and a fixed horizontal offset. ESP-IDF still restarts on
  a genuine underrun via its own `bb_eof_count` check, so recovery is retained.

Both options come from the versioned
[`esphome-mipi-rgb`](https://github.com/aclight/esphome-mipi-rgb) external
component. `alarmclock-p4-5inch.yaml` pins the ESPHome 2026.9.1 release tag.
The S3 config deliberately does not use it — see the note at the end of this
section.

### If tearing comes back: `CONFIG_CACHE_L2_CACHE_256KB`

Every bounce-buffer refill ends with an L2 cache preload of the whole buffer. At
40 scanlines that is 800 × 40 × 2 = 64 KB, which evicts half of the P4's default
128 KB L2 cache roughly every 1.8 ms, slowing down everything else including the
refills themselves.

This is the **first thing to try if tearing returns**, because it is the one
setting that was present throughout the original validation but was left out of
the production config on purpose, to find out whether it was actually needed.
Add it to the existing `sdkconfig_options` in `alarmclock-p4-5inch.yaml`:

```yaml
esp32:
  framework:
    sdkconfig_options:
      CONFIG_ESP_MAIN_TASK_STACK_SIZE: "8192"
      CONFIG_CACHE_L2_CACHE_256KB: "y"   # add this line
```

It costs 128 KB of the P4's 768 KB L2MEM. The mutually exclusive alternatives
are `CONFIG_CACHE_L2_CACHE_128KB` (the ESP-IDF default) and
`CONFIG_CACHE_L2_CACHE_512KB`.

If that is not enough, try `preferences: flash_write_interval: 1h` — flash
writes briefly disable the cache and the default writes every 60 s.

The symptom tells you which half regressed: **tearing** points at bounce-buffer
starvation, so start with the cache and `bounce_buffer_lines`. A **shifted or
wrapped** image points at the restart path instead, which `force_restart: false`
should have removed entirely rather than merely widened the margin on.

### Upgrading ESPHome

The external `esphome-mipi-rgb` component is a modified copy of ESPHome's
built-in component and is currently pinned to **2026.9.1**. ESPHome's
external-component loader installs itself at the front of `sys.meta_path`, so
the external copy **always shadows the built-in one**. Bumping the ESPHome
version therefore never picks up upstream changes to `mipi_rgb`; the fork and
the configuration's release tag have to be updated deliberately.

1. In the external component repository, diff upstream's
  `esphome/components/mipi_rgb/` at the new version against the 2026.9.1
  baseline and reapply the fork's documented changes. That component has
  changed rarely, so this is usually a small job.
2. Make the upgrade its own change with its own soak. Do not combine it with
   hardware or display-config changes — the two knobs above took a 2×2 matrix on
   real hardware to separate, and that is not worth repeating.
3. Keep `force_restart` even once it looks redundant. Upstream removed the P4
   restart in esphome/esphome#18929 (first released in 2026.9.0b1), but only
   incidentally, as a side effect of unrelated ESP32-S31 work — so it is not
   documented behaviour to rely on, and it is still needed on the S3.
4. `bounce_buffer_lines` has no upstream equivalent; upstream still hardcodes
   `width * 10`. Until that is upstreamed, the fork stays necessary.

### ESP32-S3

The 4.3-inch S3 config uses stock upstream behaviour and is untouched by the
above. It takes a different restart path inside ESP-IDF
(`RGB_LCD_NEEDS_SEPARATE_RESTART_LINK`), and upstream deliberately kept the
per-VSYNC restart there, so `force_restart: false` on the S3 is an untested
hypothesis rather than a port of a known-good fix.

## Open Questions

- Optimal PCLK frequency for 800×480 @ 60 Hz — Elecrow examples suggest 12–16 MHz.
- Light sensing strategy — BH1750 over I2C, or alternative approach.
- Backlight control on V1.1 — verify whether it uses I2C (STC8H1K28 @ 0x30) like V1.2, or GPIO PWM.

## Project Structure

```
alarmclock.yaml              # Shared application and 4.3-inch S3 entry
alarmclock-p4-5inch.yaml     # 5-inch P4 hardware overrides
secrets.yaml                 # Wi-Fi credentials, API keys (git-ignored)
components/alarmclock/       # Custom ESPHome component
  __init__.py                # Component registration
  alarmclock.h               # Header: constants, pure functions, component class
  alarmclock.cpp             # Implementation
  alarm_time.h               # Alarm time representation and scheduling
  alarm_state.h              # Alarm state machine
tests/
  test_framework.h           # Minimal test macros
  test_alarmclock.cpp        # Host-side unit tests
```

## Testing

Pure-logic functions (alarm scheduling, time calculations, backlight computation, snooze math) are tested on the host without ESPHome/Arduino dependencies.

```bash
# compile and run
g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -I . tests/test_alarmclock.cpp -o run_tests
./run_tests
```

Tests run automatically on every push/PR via GitHub Actions. CI also compiles
both ESPHome hardware configurations.

## Getting Started

```bash
# 1. Copy secrets template
cp secrets.yaml.example secrets.yaml
# 2. Edit secrets.yaml with your Wi-Fi credentials and API keys
# 3. Compile and flash the 4.3-inch ESP32-S3 board
esphome run alarmclock.yaml

# Or compile and flash the 5-inch ESP32-P4 board
esphome run alarmclock-p4-5inch.yaml
```

## Licensing

This project is MIT licensed (see `LICENSE`), with these exceptions:

- Bundled fonts are licensed under the SIL Open Font License; see
  `LICENSES/OFL-1.1.txt`.
- The icon fonts (`components/alarmclock/icon_font_*.cpp`, generated by
  `scripts/gen_icon_font.py`) contain Material Design Icons, licensed under
  Apache 2.0; see `LICENSES/Apache-2.0-MDI.txt`.
