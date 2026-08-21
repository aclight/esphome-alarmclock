# Alarm Clock — Code Review & Fix Plan

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
