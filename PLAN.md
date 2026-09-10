# Alarm Clock — Code Review & Fix Plan

## Custom Sound Assets, SD Card, and Skip-Next-Alarm


### Next up — Embedded red-tailed hawk alarm sound
RTTTL can only synthesize monophonic tones, so a real red-tailed hawk call
needs sampled PCM audio played directly through the existing `i2s_audio` speaker,
bypassing the `rtttl` component for that one sound. Build this so the playback
mechanism is reusable once SD-card-based tones (below) exist, rather than a
one-off hack:

- Add a `SoundKind` tag (`kRtttl`, `kSample`) to the alarm-sound catalog entry
  alongside the existing `name`/`rtttl` fields, so `kAlarmSounds[]` can mix RTTTL
  melodies and embedded samples. Bump `kAlarmSoundCount` /
  `kMaxStoredSoundIndex` together as already documented in `storage.h`; no
  storage format change needed since it's still just an index.
- Give `AlarmClockComponent` direct access to the `speaker::Speaker*` (a new
  `set_speaker()` setter wired from YAML, similar to the existing `set_rtttl()`)
  so the sample path can call `speaker_->set_volume()` / `speaker_->play()`
  directly instead of going through `rtttl_`.
- Write the chunked-streaming state machine (how much of the buffer to send per
  `loop()` tick, when to loop back with `kAlarmPauseDurationMs` between plays for
  a firing alarm vs. play-once for a preview) as a **pure, host-testable helper**
  in `alarmclock.h`, decoupled from ESP-IDF — mirrors how `alarm_state.h` is
  tested today. Model its data source as a small "give me the next chunk"
  interface/callback rather than hard-coding a flat in-memory array, so an
  SD-file reader can plug into the same state machine later instead of
  duplicating it.
- Add a small offline conversion script (`scripts/wav_to_pcm.py`) that converts
  a source clip to raw 16 kHz/16-bit mono PCM and emits a `const uint8_t[]`
  header — matches the
  existing `speaker:`/`rtttl:` sample rate so no resampling is needed at
  runtime. A short 1–2s clip is only tens of KB, trivial for the 16 MB flash.
- Wire `preview_sound()` / `start_alarm_sound_()` to branch on `SoundKind`
  instead of assuming RTTTL.
- **Open decision:** confirm the source/license for the red-tailed hawk audio
  clip before distributing the generated header.
- **Open decision:** exact suffix/label conventions if more embedded-sample
  sounds get added later (avoid this becoming a one-off special case in the
  catalog).

### Research items — SD card (wallpaper + extra alarm tones via Home Assistant)
Bigger, multi-stage effort. Do the hardware confirmation first; everything else
is blocked on it.

1. **Complete (hardware research):** both boards expose a physical card slot,
  but they use different interfaces. The official sources are the [P4 wiki]
  (https://www.elecrow.com/wiki/CrowPanel_Advanced_5inch_ESP32-P4_HMI_AI_Display_800x480_IPS_Touch_Screen_with_WiFi_6.html),
  [P4 repository]
  (https://github.com/Elecrow-RD/-CrowPanel-Advanced-5inch-ESP32-P4-HMI-AI-Display-800x480-IPS-Touch-Screen),
  [S3 wiki]
  (https://www.elecrow.com/wiki/CrowPanel_Advance_4.3-HMI_ESP32_AI_Display.html),
  and [S3 repository]
  (https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480).
  The P4 slot is documented as `SD1_CMD=GPIO44`, `SD1_SCK=GPIO43`,
  `SD1_D0=GPIO39`, with `CS=GND`; the lack of D1-D3 indicates SDMMC 1-bit
  wiring, although Elecrow does not state that mode explicitly. The P4 board
  in hand is V1.0, matching the documented sources. The S3 slot is SPI:
  `MOSI=GPIO6`, `MISO=GPIO4`, `SCK=GPIO5`, with CS fixed to 3.3 V / not routed
  to an ESP32 GPIO. The S3 repository documents V1.3 as the latest revision.
2. Mount the card with ESPHome's `sd_mmc_card` (or SPI-mode SD) component; get a
   basic file read working as a standalone proof before touching the UI.
3. **Decision needed:** wallpaper image pipeline. Check whether this ESPHome/
   LVGL version can decode a JPEG/PNG read live from the mounted filesystem, or
   whether a custom `lv_fs` driver bridging LVGL to the SD FAT filesystem is
   required for runtime-swappable wallpaper (vs. only compile-time-baked
   images).
4. **Decision needed:** SD-stored alarm-tone format. Start with WAV/raw PCM at
   16 kHz mono (reuses the sample-playback state machine from the
  hawk-call work above with an SD-file "next chunk" source instead of
   an in-flash array); revisit compressed formats (mp3/flac via ESPHome's
   `audio` component) later if SD/flash space becomes a real constraint.
5. **Decision needed:** upload mechanism. Home Assistant has no generic
   "upload a file to an ESP32's SD card" primitive. Start with ESPHome's
   built-in web-server file manager/upload UI (bypasses HA entirely, matches
   the "first try" scope); treat a true HA-integrated upload (proxy script +
   HA file-upload helper/card, or a custom HA integration) as a stretch goal.

Suggested order once unblocked: (a) confirm SD hardware/pinout, (b) mount +
read a test file, (c) wallpaper from a hardcoded SD path, (d) SD-based alarm
tones reusing the sample-playback state machine, (e) upload UI. Each stage is
sized for its own PR/session rather than one large change.

---

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
- WAV/MP3 alarm sounds with custom partition table — superseded by the
  "Custom Sound Assets, SD Card, and Skip-Next-Alarm" plan above
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
