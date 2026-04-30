# esphome-alarmclock

Standalone bedside alarm clock built on the **Elecrow CrowPanel Advance 4.3"** (ESP32-S3, 800×480 capacitive touch IPS display) running **ESPHome** with an **LVGL** touchscreen UI.

## Hardware

| Component | Detail |
|---|---|
| Board | Elecrow CrowPanel Advance 4.3" (ESP32-S3-WROOM-1, 16 MB flash, 8 MB PSRAM) |
| Display | 4.3" 800×480 IPS with capacitive touch (GT911) |
| Audio | MAX98357A I2S amplifier → speaker |
| Sensors | On-board light sensor (ADC); optional I²C temperature/humidity |
| Connectivity | Wi-Fi, Bluetooth LE |

## Testing

Pure-logic functions (alarm scheduling, time calculations, state machines) are tested on the host without ESPHome/Arduino dependencies.

```bash
# compile and run
g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST tests/test_alarm.cpp -o run_tests
./run_tests
```

Tests also run automatically on every push/PR via GitHub Actions.
