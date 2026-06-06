# Alarm Clock — Code Review & Fix Plan

## UI Navigation Refresh

### Goal
- Replace unreliable swipe-based page navigation with explicit buttons.
- Keep navigation logic centralized so page content stays maintainable.

### Phase 1
- Add `Alarms` and `Settings` buttons to the clock page.
- Add explicit `Home` navigation on the alarms and settings pages.
- Remove swipe and edge-arrow page navigation.
- Keep time picker and firing overlay behavior unchanged.

### Phase 2
- Introduce a small shared top-bar/button helper so page navigation chrome is built in one place.
- Replace generic page-index navigation calls at page level with semantic route helpers.

### Phase 3
- Revisit page organization if settings grows further.
- Consider splitting settings into sections only if real usage justifies it.

---

### Future (not in this plan)
- WAV/MP3 alarm sounds with custom partition table
- HA sync (restore alarms from HA entities on boot)
- Weather display on clock face
- Sunrise alarm (gradually increase brightness before alarm time)
---

## Brightness / Screen Dim-on-Wake Investigation

### Issue
After screen idle timeout, sometimes tapping to wake causes screen to get dimmer instead of restoring expected brightness. Possibly related to light sensor readings or positioning. **Not believed to be a regression from alarm changes.**

### Investigation Plan
1. Add logging to `update_backlight_()` in `alarmclock.cpp`:
   - Log current ambient lux (from sensor factor)
   - Log calculated brightness before applying
   - Log final applied brightness to controller
   - Log state transitions: screen_asleep → wake, brightness changes

2. Add logging for touch wake events:
   - Log tap event timestamp + position
   - Log screen wake request fired from touch handler
   - Log screen_asleep_ flag state transitions

3. Collect logs during testing:
   - Place device in daylight indoors
   - Let screen dim from idle (note timestamp)
   - Tap screen (note action)
   - Capture log output showing brightness calculations + sensor readings

4. Analysis:
   - Correlate sensor lux spikes/drops with dimming events
   - Check if brightness calculation is clamping unexpectedly
   - Verify screen_asleep_ transitions are correct
   - Check if light sensor readings are noisy or if positioning affects them

### Next Steps
Once logging added, run manual test, collect log segment, and debug root cause.