// Host-side tests for alarm clock logic.
// Build: g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -I . tests/test_alarmclock.cpp -o run_tests && ./run_tests

#include "tests/test_framework.h"
#include "components/alarmclock/alarmclock.h"

using namespace alarmclock;
using namespace alarm_clock;

// ---------------------------------------------------------------------------
// Smoke test — proves the test harness itself works.
// ---------------------------------------------------------------------------

TEST(smoke_test) {
    ASSERT_EQ(1, 1);
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    PASS();
}

// ===========================================================================
// Backlight computation tests
// ===========================================================================

TEST(backlight_dark_returns_min) {
    // Very low lux → dimmest backlight (kBacklightMin = 244).
    ASSERT_EQ(compute_backlight_value(0.0f, 10.0f, 500.0f), kBacklightMin);
    ASSERT_EQ(compute_backlight_value(5.0f, 10.0f, 500.0f), kBacklightMin);
    ASSERT_EQ(compute_backlight_value(10.0f, 10.0f, 500.0f), kBacklightMin);
    PASS();
}

TEST(backlight_bright_returns_max) {
    // Very high lux → brightest backlight (kBacklightMax = 0).
    ASSERT_EQ(compute_backlight_value(500.0f, 10.0f, 500.0f), kBacklightMax);
    ASSERT_EQ(compute_backlight_value(1000.0f, 10.0f, 500.0f), kBacklightMax);
    PASS();
}

TEST(backlight_midpoint) {
    // Halfway between min and max lux should give ~122.
    uint8_t val = compute_backlight_value(255.0f, 10.0f, 500.0f);
    ASSERT_TRUE(val > 100 && val < 140);
    PASS();
}

TEST(backlight_invalid_range) {
    // max_lux <= min_lux → returns kBacklightMin.
    ASSERT_EQ(compute_backlight_value(100.0f, 500.0f, 10.0f), kBacklightMin);
    ASSERT_EQ(compute_backlight_value(100.0f, 100.0f, 100.0f), kBacklightMin);
    PASS();
}

TEST(backlight_constants_sanity) {
    ASSERT_EQ(kBacklightMax, 0);
    ASSERT_EQ(kBacklightMin, 244);
    ASSERT_EQ(kBacklightOff, 245);
    ASSERT_EQ(kBuzzerOn, 246);
    ASSERT_EQ(kBuzzerOff, 247);
    PASS();
}

// ===========================================================================
// Simple alarm matching tests (hour/minute only)
// ===========================================================================

TEST(alarm_matches_simple_exact) {
    ASSERT_TRUE(alarm_matches(7, 30, 7, 30));
    PASS();
}

TEST(alarm_matches_simple_miss_minute) {
    ASSERT_FALSE(alarm_matches(7, 30, 7, 31));
    PASS();
}

TEST(alarm_matches_simple_miss_hour) {
    ASSERT_FALSE(alarm_matches(7, 30, 8, 30));
    PASS();
}

TEST(alarm_matches_simple_midnight) {
    ASSERT_TRUE(alarm_matches(0, 0, 0, 0));
    ASSERT_FALSE(alarm_matches(0, 0, 23, 59));
    PASS();
}

// ===========================================================================
// Snooze time computation tests
// ===========================================================================

TEST(snooze_normal) {
    auto [h, m] = compute_snooze_time(7, 30, 9);
    ASSERT_EQ(h, 7);
    ASSERT_EQ(m, 39);
    PASS();
}

TEST(snooze_rollover_past_midnight) {
    auto [h, m] = compute_snooze_time(23, 55, 9);
    ASSERT_EQ(h, 0);
    ASSERT_EQ(m, 4);
    PASS();
}

TEST(snooze_exact_midnight) {
    auto [h, m] = compute_snooze_time(23, 51, 9);
    ASSERT_EQ(h, 0);
    ASSERT_EQ(m, 0);
    PASS();
}

TEST(snooze_zero_duration) {
    auto [h, m] = compute_snooze_time(12, 0, 0);
    ASSERT_EQ(h, 12);
    ASSERT_EQ(m, 0);
    PASS();
}

TEST(snooze_large_duration) {
    // 23:00 + 120 min = 01:00 next day.
    auto [h, m] = compute_snooze_time(23, 0, 120);
    ASSERT_EQ(h, 1);
    ASSERT_EQ(m, 0);
    PASS();
}

// ===========================================================================
// AlarmTime tests (day-of-week scheduling from alarm_time.h)
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
    AlarmTime at{7, 30, kWeekdays, true};
    ASSERT_TRUE(alarm_active_on_day(at, 1));   // Mon
    ASSERT_TRUE(alarm_active_on_day(at, 5));   // Fri
    ASSERT_FALSE(alarm_active_on_day(at, 0));  // Sun
    ASSERT_FALSE(alarm_active_on_day(at, 6));  // Sat
    PASS();
}

TEST(alarm_active_on_day_disabled) {
    AlarmTime at{7, 30, kEveryDay, false};
    ASSERT_FALSE(alarm_active_on_day(at, 0));
    ASSERT_FALSE(alarm_active_on_day(at, 3));
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

TEST(alarm_matches_with_day_exact) {
    AlarmTime at{7, 30, kMonday, true};
    ASSERT_TRUE(alarm_clock::alarm_matches(at, 7, 30, 1));
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 7, 31, 1));
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 7, 30, 2));
    PASS();
}

TEST(alarm_matches_with_day_disabled) {
    AlarmTime at{7, 30, kMonday, false};
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 7, 30, 1));
    PASS();
}

TEST(minutes_until_alarm_fires_now) {
    AlarmTime at{7, 30, kMonday, true};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), 0);
    PASS();
}

TEST(minutes_until_alarm_later_today) {
    AlarmTime at{9, 0, kMonday, true};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), 90);
    PASS();
}

TEST(minutes_until_alarm_tomorrow_same_day_flag) {
    AlarmTime at{9, 0, kMonday, true};
    int32_t expected = 7 * 24 * 60 - time_to_minutes(10, 0) + time_to_minutes(9, 0);
    ASSERT_EQ(minutes_until_alarm(at, 10, 0, 1), expected);
    PASS();
}

TEST(minutes_until_alarm_next_day) {
    AlarmTime at{7, 30, kTuesday, true};
    int32_t expected = 1 * 24 * 60 - time_to_minutes(22, 0) + time_to_minutes(7, 30);
    ASSERT_EQ(minutes_until_alarm(at, 22, 0, 1), expected);
    PASS();
}

TEST(minutes_until_alarm_disabled) {
    AlarmTime at{7, 30, kMonday, false};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), -1);
    PASS();
}

TEST(minutes_until_alarm_no_days) {
    AlarmTime at{7, 30, 0, true};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), -1);
    PASS();
}

TEST(minutes_until_alarm_weekdays_on_friday_evening) {
    AlarmTime at{6, 0, kWeekdays, true};
    int32_t expected = 3 * 24 * 60 - time_to_minutes(23, 0) + time_to_minutes(6, 0);
    ASSERT_EQ(minutes_until_alarm(at, 23, 0, 5), expected);
    PASS();
}

TEST(minutes_until_alarm_every_day) {
    AlarmTime at{8, 0, kEveryDay, true};
    int32_t expected = 1 * 24 * 60 - time_to_minutes(10, 0) + time_to_minutes(8, 0);
    ASSERT_EQ(minutes_until_alarm(at, 10, 0, 3), expected);
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
// AlarmStateMachine tests (from alarm_state.h)
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
    ASSERT_FALSE(sm.snooze());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    PASS();
}

TEST(state_machine_snooze_tick_countdown) {
    AlarmStateMachine sm;
    sm.set_snooze_duration(3);
    sm.trigger();
    sm.snooze();

    ASSERT_FALSE(sm.tick_snooze());
    ASSERT_EQ(sm.snooze_remaining_minutes(), 2);

    ASSERT_FALSE(sm.tick_snooze());
    ASSERT_EQ(sm.snooze_remaining_minutes(), 1);

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
        sm.tick_snooze();
    }
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kFiring));
    ASSERT_FALSE(sm.snooze());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    PASS();
}

TEST(state_machine_tick_snooze_noop_when_not_snoozed) {
    AlarmStateMachine sm;
    ASSERT_FALSE(sm.tick_snooze());
    sm.trigger();
    ASSERT_FALSE(sm.tick_snooze());
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

    // Backlight
    RUN(backlight_dark_returns_min);
    RUN(backlight_bright_returns_max);
    RUN(backlight_midpoint);
    RUN(backlight_invalid_range);
    RUN(backlight_constants_sanity);

    // Simple alarm matching
    RUN(alarm_matches_simple_exact);
    RUN(alarm_matches_simple_miss_minute);
    RUN(alarm_matches_simple_miss_hour);
    RUN(alarm_matches_simple_midnight);

    // Snooze computation
    RUN(snooze_normal);
    RUN(snooze_rollover_past_midnight);
    RUN(snooze_exact_midnight);
    RUN(snooze_zero_duration);
    RUN(snooze_large_duration);

    // AlarmTime (day-of-week scheduling)
    RUN(day_index_to_flag_mapping);
    RUN(alarm_active_on_day_enabled);
    RUN(alarm_active_on_day_disabled);
    RUN(time_to_minutes_basic);
    RUN(alarm_matches_with_day_exact);
    RUN(alarm_matches_with_day_disabled);
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
