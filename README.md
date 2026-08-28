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

## Software Stack

- **ESPHome** (`esp-idf` framework) — core firmware, OTA, Home Assistant API
- **LVGL** — touchscreen UI rendering
- **Custom C++ component** (`components/alarmclock/`) — alarm logic, backlight control, audio

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

Display Brightness sets the daytime level. Night Brightness sets the dark-room
level as a percentage of that daytime setting. The firmware interpolates between
them from the BH1750 reading and applies neutral content shading when the
backlight reaches the bottom of its useful range.

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
