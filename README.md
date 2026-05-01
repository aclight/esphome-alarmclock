# esphome-alarmclock

Standalone bedside alarm clock built on the **Elecrow CrowPanel Advance 4.3"** (ESP32-S3, 800×480 capacitive touch IPS display) running **ESPHome** with an **LVGL** touchscreen UI.

## Hardware

### Board

**Elecrow CrowPanel Advance 4.3 v1.1"** — ESP32-S3-WROOM-1, 16 MB flash, 8 MB PSRAM.

### Pin Map

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

The CrowPanel Advance 4.3" has a 4-position DIP switch used during flashing and boot mode selection. All switches should be OFF during normal operation.

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
- **Auto-dimming backlight** — Ambient light sensor drives LCD backlight brightness
- **Home Assistant integration** — Alarm control, sensor data, OTA updates via ESPHome API
- **LVGL touchscreen UI** — Clock faces, alarm configuration, settings screens

## Software Stack

- **ESPHome** (`esp-idf` framework) — core firmware, OTA, Home Assistant API
- **LVGL** — touchscreen UI rendering
- **Custom C++ component** (`components/alarmclock/`) — alarm logic, backlight control, audio

## Assembly Notes

1. The CrowPanel comes pre-assembled; no soldering required.
2. Power via USB-C (5 V). The board has an on-board voltage regulator.
3. Speaker connects to the on-board SPK connector (2-pin, wired to the MAX98357A).

## Open Questions

- Optimal PCLK frequency for 800×480 @ 60 Hz — Elecrow examples suggest 12–16 MHz.
- Light sensing strategy — BH1750 over I2C, or alternative approach.
- Backlight control on V1.1 — verify whether it uses I2C (STC8H1K28 @ 0x30) like V1.2, or GPIO PWM.

## Project Structure

```
alarmclock.yaml              # Main ESPHome configuration
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

Tests run automatically on every push/PR via GitHub Actions. CI also validates ESPHome YAML compilation.

## Getting Started

```bash
# 1. Copy secrets template
cp secrets.yaml.example secrets.yaml
# 2. Edit secrets.yaml with your Wi-Fi credentials and API keys
# 3. Compile and flash
esphome run alarmclock.yaml
```
