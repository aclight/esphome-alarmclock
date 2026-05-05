// Pure-logic alarm time representation and scheduling.
// Decoupled from ESPHome/Arduino so it can be tested on the host.
#ifndef ALARM_TIME_H
#define ALARM_TIME_H

#include <cstdint>
#include <cstring>

namespace alarm_clock {

// Maximum length of an alarm label (including null terminator).
static constexpr uint8_t kAlarmLabelMaxLen = 16;

// Days of the week as bit flags.
enum DayOfWeek : uint8_t {
  kSunday    = 1 << 0,
  kMonday    = 1 << 1,
  kTuesday   = 1 << 2,
  kWednesday = 1 << 3,
  kThursday  = 1 << 4,
  kFriday    = 1 << 5,
  kSaturday  = 1 << 6,
};

static constexpr uint8_t kWeekdays =
    kMonday | kTuesday | kWednesday | kThursday | kFriday;
static constexpr uint8_t kWeekends = kSaturday | kSunday;
static constexpr uint8_t kEveryDay = kWeekdays | kWeekends;

// Represents a single alarm configuration.
struct AlarmTime {
  uint8_t hour = 0;        // 0–23
  uint8_t minute = 0;      // 0–59
  uint8_t days_of_week = 0; // bitmask of DayOfWeek
  bool enabled = false;
  char label[kAlarmLabelMaxLen] = "";  // User-defined label (e.g. "Work")
};

// Set the label on an alarm, safely truncating to kAlarmLabelMaxLen-1 chars.
inline void alarm_set_label(AlarmTime &alarm, const char *text) {
  if (text == nullptr) {
    alarm.label[0] = '\0';
    return;
  }
  std::strncpy(alarm.label, text, kAlarmLabelMaxLen - 1);
  alarm.label[kAlarmLabelMaxLen - 1] = '\0';
};

// Convert a day-of-week index (0 = Sunday … 6 = Saturday) to its flag bit.
inline uint8_t day_index_to_flag(uint8_t day_index) {
  return static_cast<uint8_t>(1 << (day_index % 7));
}

// Check whether an alarm is active on a given day index (0 = Sun … 6 = Sat).
inline bool alarm_active_on_day(const AlarmTime &alarm, uint8_t day_index) {
  return alarm.enabled && (alarm.days_of_week & day_index_to_flag(day_index));
}

// Return the time-of-day in minutes (0–1439).
inline uint16_t time_to_minutes(uint8_t hour, uint8_t minute) {
  return static_cast<uint16_t>(hour) * 60 + minute;
}

// Check whether the alarm matches a given time exactly (hour, minute, day).
inline bool alarm_matches(const AlarmTime &alarm, uint8_t hour, uint8_t minute,
                          uint8_t day_index) {
  return alarm_active_on_day(alarm, day_index) && alarm.hour == hour &&
         alarm.minute == minute;
}

// Compute the number of minutes until the next occurrence of |alarm|.
// |now_hour|, |now_minute|: current time of day.
// |now_day_index|: current day-of-week (0 = Sunday … 6 = Saturday).
// Returns 0 if the alarm fires right now.
// Returns -1 if the alarm is disabled or has no active days.
inline int32_t minutes_until_alarm(const AlarmTime &alarm, uint8_t now_hour,
                                   uint8_t now_minute, uint8_t now_day_index) {
  if (!alarm.enabled || alarm.days_of_week == 0) {
    return -1;
  }

  const uint16_t now_mins = time_to_minutes(now_hour, now_minute);
  const uint16_t alarm_mins = time_to_minutes(alarm.hour, alarm.minute);

  // Check up to 8 offsets: today through same-day-next-week.
  for (uint8_t offset = 0; offset <= 7; ++offset) {
    uint8_t check_day = static_cast<uint8_t>((now_day_index + offset) % 7);
    if (!(alarm.days_of_week & day_index_to_flag(check_day))) {
      continue;
    }
    if (offset == 0) {
      // Same day: alarm must be now or in the future.
      if (alarm_mins >= now_mins) {
        return static_cast<int32_t>(alarm_mins - now_mins);
      }
      // Already passed today; continue to next matching day.
    } else {
      // Future day.
      int32_t day_minutes = static_cast<int32_t>(offset) * 24 * 60;
      return day_minutes - static_cast<int32_t>(now_mins) +
             static_cast<int32_t>(alarm_mins);
    }
  }

  // All 7 days checked but alarm always in the past on those days?
  // This shouldn't happen if days_of_week != 0, but handle gracefully.
  return -1;
}

}  // namespace alarm_clock

#endif  // ALARM_TIME_H
