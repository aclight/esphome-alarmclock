// Host-side tests for alarm clock logic.
// Build: g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -I.. test_alarm.cpp -o run_tests && ./run_tests

#include "test_framework.h"
#include "components/alarmclock/alarm_time.h"
#include "components/alarmclock/alarm_state.h"

using namespace alarmclock;// ---------------------------------------------------------------------------
// Smoke test — proves the test harness itself works.
// ---------------------------------------------------------------------------

TEST(smoke_test) {
    ASSERT_EQ(1, 1);
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    PASS();
}

// ===========================================================================
// AlarmTime tests
// ===========================================================================

TEST(day_index_to_flag_mapping) {
    ASSERT_EQ(day_index_to_flag(0), kSunday);
    ASSERT_EQ(day_index_to_flag(1), kMonday);
    ASSERT_EQ(day_index_to_flag(2), kTuesday);
    ASSERT_EQ(day_index_to_flag(3), kWednesday);
    ASSERT_EQ(day_index_to_flag(4), kThursday);
    ASSERT_EQ(day_index_to_flag(5), kFriday);
    ASSERT_EQ(day_index_to_flag(6), kSaturday);
    PASS();
}

TEST(alarm_active_on_day_enabled) {
    AlarmTime alarm{7, 30, kWeekdays, true};
    // Monday–Friday should be active.
    ASSERT_TRUE(alarm_active_on_day(alarm, 1));  // Mon
    ASSERT_TRUE(alarm_active_on_day(alarm, 5));  // Fri
    // Weekend should not.
    ASSERT_FALSE(alarm_active_on_day(alarm, 0)); // Sun
    ASSERT_FALSE(alarm_active_on_day(alarm, 6)); // Sat
    PASS();
}

TEST(alarm_active_on_day_disabled) {
    AlarmTime alarm{7, 30, kEveryDay, false};
    // Disabled alarm is never active.
    ASSERT_FALSE(alarm_active_on_day(alarm, 0));
    ASSERT_FALSE(alarm_active_on_day(alarm, 3));
    PASS();
}

TEST(time_to_minutes_basic) {
    ASSERT_EQ(time_to_minutes(0, 0), 0);
    ASSERT_EQ(time_to_minutes(0, 1), 1);
    ASSERT_EQ(time_to_minutes(1, 0), 60);
    ASSERT_EQ(time_to_minutes(23, 59), 1439);
    ASSERT_EQ(time_to_minutes(12, 30), 750);
    PASS();
}

TEST(alarm_matches_exact) {
    AlarmTime alarm{7, 30, kMonday, true};
    ASSERT_TRUE(alarm_matches(alarm, 7, 30, 1));   // Mon 07:30
    ASSERT_FALSE(alarm_matches(alarm, 7, 31, 1));   // Wrong minute
    ASSERT_FALSE(alarm_matches(alarm, 8, 30, 1));   // Wrong hour
    ASSERT_FALSE(alarm_matches(alarm, 7, 30, 2));   // Wrong day (Tue)
    PASS();
}

TEST(alarm_matches_disabled) {
    AlarmTime alarm{7, 30, kMonday, false};
    ASSERT_FALSE(alarm_matches(alarm, 7, 30, 1));
    PASS();
}

TEST(minutes_until_alarm_fires_now) {
    AlarmTime alarm{7, 30, kMonday, true};
    // It's Monday 07:30 — should return 0.
    ASSERT_EQ(minutes_until_alarm(alarm, 7, 30, 1), 0);
    PASS();
}

TEST(minutes_until_alarm_later_today) {
    AlarmTime alarm{9, 0, kMonday, true};
    // Monday 07:30, alarm at 09:00 → 90 minutes.
    ASSERT_EQ(minutes_until_alarm(alarm, 7, 30, 1), 90);
    PASS();
}

TEST(minutes_until_alarm_tomorrow_same_day_flag) {
    // Alarm only on Monday, it's Monday 10:00, alarm is 09:00.
    // Next occurrence is next Monday = 7 days later minus 1 hour.
    AlarmTime alarm{9, 0, kMonday, true};
    int32_t expected = 7 * 24 * 60 - time_to_minutes(10, 0) + time_to_minutes(9, 0);  // 10020
    ASSERT_EQ(minutes_until_alarm(alarm, 10, 0, 1), expected);
    PASS();
}

TEST(minutes_until_alarm_next_day) {
    // Alarm on Tuesday only, it's Monday 22:00.
    // Next Tue = 1 day away → 1*1440 - 22*60 + 7*30 = 1440 - 1320 + 450 = 570
    AlarmTime alarm{7, 30, kTuesday, true};
    int32_t expected = 1 * 24 * 60 - time_to_minutes(22, 0) + time_to_minutes(7, 30);
    ASSERT_EQ(minutes_until_alarm(alarm, 22, 0, 1), expected);
    PASS();
}

TEST(minutes_until_alarm_disabled) {
    AlarmTime alarm{7, 30, kMonday, false};
    ASSERT_EQ(minutes_until_alarm(alarm, 7, 30, 1), -1);
    PASS();
}

TEST(minutes_until_alarm_no_days) {
    AlarmTime alarm{7, 30, 0, true};
    ASSERT_EQ(minutes_until_alarm(alarm, 7, 30, 1), -1);
    PASS();
}

TEST(minutes_until_alarm_weekdays_on_friday_evening) {
    // Friday 23:00, alarm at 06:00 weekdays.
    // Next weekday is Monday = 3 days away.
    AlarmTime alarm{6, 0, kWeekdays, true};
    int32_t expected = 3 * 24 * 60 - time_to_minutes(23, 0) + time_to_minutes(6, 0);
    ASSERT_EQ(minutes_until_alarm(alarm, 23, 0, 5), expected);
    PASS();
}

TEST(minutes_until_alarm_every_day) {
    // Every day alarm at 08:00, current time 10:00 Wednesday.
    // Next is tomorrow (Thu) at 08:00.
    AlarmTime alarm{8, 0, kEveryDay, true};
    int32_t expected = 1 * 24 * 60 - time_to_minutes(10, 0) + time_to_minutes(8, 0);
    ASSERT_EQ(minutes_until_alarm(alarm, 10, 0, 3), expected);
    PASS();
}

TEST(constants_weekdays) {
    ASSERT_EQ(kWeekdays, kMonday | kTuesday | kWednesday | kThursday | kFriday);
    ASSERT_FALSE(kWeekdays & kSaturday);
    ASSERT_FALSE(kWeekdays & kSunday);
    PASS();
}

TEST(constants_weekends) {
    ASSERT_EQ(kWeekends, kSaturday | kSunday);
    ASSERT_FALSE(kWeekends & kMonday);
    PASS();
}

TEST(constants_every_day) {
    ASSERT_EQ(kEveryDay, kWeekdays | kWeekends);
    for (uint8_t i = 0; i < 7; ++i) {
        ASSERT_TRUE(kEveryDay & day_index_to_flag(i));
    }
    PASS();
}

// ===========================================================================
// AlarmStateMachine tests
// ===========================================================================

TEST(state_machine_initial_state) {
    AlarmStateMachine sm;
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    ASSERT_EQ(sm.snooze_count(), 0);
    PASS();
}

TEST(state_machine_trigger) {
    AlarmStateMachine sm;
    ASSERT_TRUE(sm.trigger());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kFiring));
    // Triggering again while firing returns false.
    ASSERT_FALSE(sm.trigger());
    PASS();
}

TEST(state_machine_dismiss) {
    AlarmStateMachine sm;
    sm.trigger();
    sm.dismiss();
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    ASSERT_EQ(sm.snooze_count(), 0);
    PASS();
}

TEST(state_machine_snooze) {
    AlarmStateMachine sm;
    sm.trigger();
    ASSERT_TRUE(sm.snooze());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kSnoozed));
    ASSERT_EQ(sm.snooze_count(), 1);
    ASSERT_EQ(sm.snooze_remaining_minutes(), kDefaultSnoozeDurationMinutes);
    PASS();
}

TEST(state_machine_snooze_only_when_firing) {
    AlarmStateMachine sm;
    // Can't snooze from idle.
    ASSERT_FALSE(sm.snooze());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    PASS();
}

TEST(state_machine_snooze_tick_countdown) {
    AlarmStateMachine sm;
    sm.set_snooze_duration(3);  // 3-minute snooze for faster testing.
    sm.trigger();
    sm.snooze();

    // Tick 1: 2 minutes remaining.
    ASSERT_FALSE(sm.tick_snooze());
    ASSERT_EQ(sm.snooze_remaining_minutes(), 2);

    // Tick 2: 1 minute remaining.
    ASSERT_FALSE(sm.tick_snooze());
    ASSERT_EQ(sm.snooze_remaining_minutes(), 1);

    // Tick 3: expires → fires again.
    ASSERT_TRUE(sm.tick_snooze());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kFiring));
    ASSERT_EQ(sm.snooze_remaining_minutes(), 0);
    PASS();
}

TEST(state_machine_max_snoozes) {
    AlarmStateMachine sm;
    sm.set_snooze_duration(1);

    for (uint8_t i = 0; i < kMaxSnoozeCount; ++i) {
        sm.trigger();
        ASSERT_TRUE(sm.snooze());
        sm.tick_snooze();  // Expire immediately.
    }
    // Now at max snooze count. Next snooze should auto-dismiss.
    // State is kFiring after last tick_snooze expired.
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kFiring));
    ASSERT_FALSE(sm.snooze());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    PASS();
}

TEST(state_machine_tick_snooze_noop_when_not_snoozed) {
    AlarmStateMachine sm;
    ASSERT_FALSE(sm.tick_snooze());  // Idle — no-op.
    sm.trigger();
    ASSERT_FALSE(sm.tick_snooze());  // Firing — no-op.
    PASS();
}

TEST(state_machine_reset) {
    AlarmStateMachine sm;
    sm.trigger();
    sm.snooze();
    sm.reset();
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    ASSERT_EQ(sm.snooze_count(), 0);
    ASSERT_EQ(sm.snooze_remaining_minutes(), 0);
    PASS();
}

TEST(state_machine_custom_snooze_duration) {
    AlarmStateMachine sm;
    sm.set_snooze_duration(15);
    sm.trigger();
    sm.snooze();
    ASSERT_EQ(sm.snooze_remaining_minutes(), 15);
    PASS();
}

// ---------------------------------------------------------------------------
// main — register every TEST here.
// ---------------------------------------------------------------------------

int main() {
    printf("Running alarm-clock tests...\n");

    // Smoke
    RUN(smoke_test);

    // AlarmTime
    RUN(day_index_to_flag_mapping);
    RUN(alarm_active_on_day_enabled);
    RUN(alarm_active_on_day_disabled);
    RUN(time_to_minutes_basic);
    RUN(alarm_matches_exact);
    RUN(alarm_matches_disabled);
    RUN(minutes_until_alarm_fires_now);
    RUN(minutes_until_alarm_later_today);
    RUN(minutes_until_alarm_tomorrow_same_day_flag);
    RUN(minutes_until_alarm_next_day);
    RUN(minutes_until_alarm_disabled);
    RUN(minutes_until_alarm_no_days);
    RUN(minutes_until_alarm_weekdays_on_friday_evening);
    RUN(minutes_until_alarm_every_day);
    RUN(constants_weekdays);
    RUN(constants_weekends);
    RUN(constants_every_day);

    // AlarmStateMachine
    RUN(state_machine_initial_state);
    RUN(state_machine_trigger);
    RUN(state_machine_dismiss);
    RUN(state_machine_snooze);
    RUN(state_machine_snooze_only_when_firing);
    RUN(state_machine_snooze_tick_countdown);
    RUN(state_machine_max_snoozes);
    RUN(state_machine_tick_snooze_noop_when_not_snoozed);
    RUN(state_machine_reset);
    RUN(state_machine_custom_snooze_duration);

    printf("\n%d test(s) run, %d failed.\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
