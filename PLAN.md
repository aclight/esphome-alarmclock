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
