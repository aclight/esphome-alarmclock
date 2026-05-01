// Pure-logic alarm state machine.
// Decoupled from ESPHome/Arduino so it can be tested on the host.
#ifndef ALARM_STATE_H
#define ALARM_STATE_H

#include <cstdint>

namespace alarm_clock {

enum class AlarmState : uint8_t {
  kIdle,      // Waiting for alarm time.
  kFiring,    // Alarm is sounding.
  kSnoozed,   // Alarm was snoozed; will fire again after snooze expires.
};

// Default snooze duration in minutes.
static constexpr uint8_t kDefaultSnoozeDurationMinutes = 9;

// Maximum number of snoozes before auto-dismiss.
static constexpr uint8_t kMaxSnoozeCount = 3;

// Manages the runtime state of a single alarm.
class AlarmStateMachine {
 public:
  AlarmState state() const { return state_; }
  uint8_t snooze_count() const { return snooze_count_; }
  uint8_t snooze_duration_minutes() const { return snooze_duration_minutes_; }
  uint16_t snooze_remaining_minutes() const {
    return snooze_remaining_minutes_;
  }

  void set_snooze_duration(uint8_t minutes) {
    snooze_duration_minutes_ = minutes;
  }

  // Trigger the alarm (called when alarm time matches current time).
  // Returns true if state actually changed.
  bool trigger() {
    if (state_ == AlarmState::kFiring) {
      return false;  // Already firing.
    }
    state_ = AlarmState::kFiring;
    return true;
  }

  // Snooze the alarm. Returns true if snooze was accepted.
  bool snooze() {
    if (state_ != AlarmState::kFiring) {
      return false;
    }
    if (snooze_count_ >= kMaxSnoozeCount) {
      // Too many snoozes — dismiss instead.
      dismiss();
      return false;
    }
    snooze_count_++;
    snooze_remaining_minutes_ = snooze_duration_minutes_;
    state_ = AlarmState::kSnoozed;
    return true;
  }

  // Dismiss the alarm entirely.
  void dismiss() {
    state_ = AlarmState::kIdle;
    snooze_count_ = 0;
    snooze_remaining_minutes_ = 0;
  }

  // Called once per minute while snoozed to tick down the snooze timer.
  // Returns true if the snooze expired and the alarm should fire again.
  bool tick_snooze() {
    if (state_ != AlarmState::kSnoozed) {
      return false;
    }
    if (snooze_remaining_minutes_ > 0) {
      snooze_remaining_minutes_--;
    }
    if (snooze_remaining_minutes_ == 0) {
      state_ = AlarmState::kFiring;
      return true;
    }
    return false;
  }

  // Reset all state (e.g. when alarm config changes).
  void reset() {
    state_ = AlarmState::kIdle;
    snooze_count_ = 0;
    snooze_remaining_minutes_ = 0;
  }

 private:
  AlarmState state_ = AlarmState::kIdle;
  uint8_t snooze_count_ = 0;
  uint8_t snooze_duration_minutes_ = kDefaultSnoozeDurationMinutes;
  uint16_t snooze_remaining_minutes_ = 0;
};

}  // namespace alarm_clock

#endif  // ALARM_STATE_H
