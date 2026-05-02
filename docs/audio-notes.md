# CrowPanel Advance 4.3" - Audio/Speaker Notes

## Hardware
- MAX98357A I2S amplifier connected to speaker
- I2S pins: DOUT=GPIO4, BCLK=GPIO5, LRCLK=GPIO6
- These pins are shared with SPI/SD card (HSPI_MISO=4, HSPI_MOSI=6, HSPI_SCLK=5)
- DIP switches on back must be set to MIC&SPK mode (0, 0) for speaker to work

## Speaker Amplifier Enable (CRITICAL)
- The speaker amp is **disabled by default** on boot
- Must send I2C command **248** to address **0x30** (STC8H1K28 microcontroller) to enable it
- Without this command, I2S data is sent but no sound comes out
- Same microcontroller controls backlight (0=max brightness, 245=off)
- Command 250 = activate touch screen
- Reference: Elecrow's Arduino lesson-02 (OnlineAudio_Large.ino)

## ESPHome Speaker Config
```yaml
i2s_audio:
  i2s_lrclk_pin: GPIO6
  i2s_bclk_pin: GPIO5

speaker:
  - platform: i2s_audio
    id: alarm_speaker
    dac_type: external
    i2s_dout_pin: GPIO4
    channel: mono
    timeout: never
    sample_rate: 16000
    use_apll: true
```

## Volume Control
- `speaker.volume_set` does NOT effectively control RTTTL playback volume
- RTTTL generates waveform at its own gain level, independent of speaker volume
- Use `id(alarm_rtttl).set_gain(float)` to control RTTTL volume (0.0 to 1.0)
- Must call `set_gain()` before `rtttl.play` for it to take effect on that playback
- Can also call `set_gain()` mid-playback and it updates immediately

## Boot Sequence (on_boot, priority -100)
```cpp
uint8_t cmd = 248;
auto err = id(i2c_bus).write(0x30, &cmd, 1);
```

## Elecrow Official Example
- Repo: github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480
- Audio example: example/V1.1_and_V1.2/arduino/lesson-02/OnlineAudio_large/
- Uses ESP32-audioI2S library for MP3 streaming
