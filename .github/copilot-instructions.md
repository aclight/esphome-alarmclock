# Copilot Instructions

## Project Context

This is a standalone bedside alarm clock built on the **Elecrow CrowPanel Advance 4.3"** (ESP32-S3, 800×480 capacitive touch IPS display). The firmware runs **ESPHome** (using the `esp-idf` framework) with **LVGL** for the touchscreen UI and integrates with **Home Assistant**.

### Hardware

| Component | Detail |
|---|---|
| Board | Elecrow CrowPanel Advance 4.3" (ESP32-S3-WROOM-1, 16 MB flash, 8 MB PSRAM) |
| Display | 4.3" 800×480 IPS with `rpi_dpi_rgb` driver, capacitive touch (GT911 @ 0x5D) |
| Audio | MAX98357A I2S amplifier → speaker (DOUT=IO4, BCLK=IO5, LRCLK=IO6) |
| Sensors | Optional BH1750 I²C light sensor (0x23) |
| I2C bus | SDA=IO15, SCL=IO16 |
| Connectivity | Wi-Fi (built-in), Bluetooth LE (built-in) |

### Software Stack

- **ESPHome** — core firmware framework, OTA updates, Home Assistant integration
- **LVGL** — touchscreen UI (clock faces, alarm configuration, settings)
- Custom C++ component under `components/alarmclock/` for alarm logic, backlight control, audio playback, and sensor handling

## ESPHome YAML

- The main config is `alarmclock.yaml` using the `esp-idf` framework (not Arduino).
- Display uses `rpi_dpi_rgb` platform — exact data pin mapping is board-version-sensitive; verify against Elecrow's GitHub examples.
- `external_components` points to `components/alarmclock/` for the custom component.
- Secrets (Wi-Fi, API key, OTA password) live in `secrets.yaml` (git-ignored); a template is provided in `secrets.yaml.example`.

## C/C++ Coding Style

Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with the following rules emphasized:

- **Always use curly braces** for `if`, `else`, `for`, `while`, and `do` bodies — even single-line bodies. No braceless control structures.
- Use `snake_case` for variables and functions, `kConstantName` for constants, `ClassName` for types.
- Prefer `const` wherever applicable.
- Use `uint8_t`, `uint16_t`, etc. (from `<cstdint>`) instead of bare `int` for sized fields.

## Testing

- **Write or update tests** in `tests/test_alarmclock.cpp` for every change to testable logic in `components/alarmclock/alarmclock.h` or supporting headers.
- Keep pure-logic functions (time calculations, alarm scheduling, backlight computation, snooze math, state machine logic) decoupled from ESPHome/Arduino APIs so they can compile and run on the host with `g++` under `-DUNIT_TEST`.
- Use the `TEST(name)` / `ASSERT_EQ` / `ASSERT_TRUE` / `ASSERT_FALSE` / `PASS()` macros defined in `tests/test_framework.h`.
- Tests are compiled with `-Wall -Wextra -Werror`; all warnings must be clean.
- CI runs tests automatically on every push and pull request via GitHub Actions (`.github/workflows/test.yml`).
- CI also validates that `alarmclock.yaml` compiles with ESPHome.
