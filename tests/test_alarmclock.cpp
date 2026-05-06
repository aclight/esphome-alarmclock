// Host-side tests for alarm clock logic.
// Build: g++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -I . tests/test_alarmclock.cpp -o run_tests && ./run_tests

#include "tests/test_framework.h"
#include "components/alarmclock/alarmclock.h"
#include "components/alarmclock/storage.h"
#include <cstring>

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
// lux_to_sensor_factor tests
// ===========================================================================

TEST(sensor_factor_dark) {
    ASSERT_TRUE(lux_to_sensor_factor(0.0f, 10.0f, 500.0f) == 0.0f);
    ASSERT_TRUE(lux_to_sensor_factor(10.0f, 10.0f, 500.0f) == 0.0f);
    PASS();
}

TEST(sensor_factor_bright) {
    ASSERT_TRUE(lux_to_sensor_factor(500.0f, 10.0f, 500.0f) == 1.0f);
    ASSERT_TRUE(lux_to_sensor_factor(1000.0f, 10.0f, 500.0f) == 1.0f);
    PASS();
}

TEST(sensor_factor_midpoint) {
    float sf = lux_to_sensor_factor(255.0f, 10.0f, 500.0f);
    ASSERT_TRUE(sf > 0.45f && sf < 0.55f);
    PASS();
}

TEST(sensor_factor_invalid_range) {
    ASSERT_TRUE(lux_to_sensor_factor(100.0f, 500.0f, 10.0f) == 0.0f);
    ASSERT_TRUE(lux_to_sensor_factor(100.0f, 100.0f, 100.0f) == 0.0f);
    PASS();
}

// ===========================================================================
// compute_brightness tests
// ===========================================================================

TEST(brightness_user_high_sensor_bright) {
    // user=1.0, sensor=1.0 → top of highest window → 1.0
    float b = compute_brightness(1.0f, 1.0f, 0.5f, 0.1f, 1.0f);
    ASSERT_TRUE(b > 0.99f && b <= 1.0f);
    PASS();
}

TEST(brightness_user_high_sensor_dark) {
    // user=1.0, sensor=0.0 → bottom of highest window → 0.5
    float b = compute_brightness(1.0f, 0.0f, 0.5f, 0.1f, 1.0f);
    ASSERT_TRUE(b > 0.49f && b < 0.51f);
    PASS();
}

TEST(brightness_user_low_sensor_bright) {
    // user=0.0, sensor=1.0 → top of lowest window → 0.6
    float b = compute_brightness(0.0f, 1.0f, 0.5f, 0.1f, 1.0f);
    ASSERT_TRUE(b > 0.59f && b < 0.61f);
    PASS();
}

TEST(brightness_user_low_sensor_dark) {
    // user=0.0, sensor=0.0 → bottom of lowest window → 0.1
    float b = compute_brightness(0.0f, 0.0f, 0.5f, 0.1f, 1.0f);
    ASSERT_TRUE(b > 0.09f && b < 0.11f);
    PASS();
}

TEST(brightness_user_mid_sensor_mid) {
    // user=0.5, sensor=0.5 → middle of middle window → 0.55
    float b = compute_brightness(0.5f, 0.5f, 0.5f, 0.1f, 1.0f);
    ASSERT_TRUE(b > 0.54f && b < 0.56f);
    PASS();
}

TEST(brightness_no_sensor) {
    // Without sensor, pass sensor_factor=1.0 → gets window's upper bound.
    // Defaults: auto_range=0.5, min=0.0, max=1.0
    // user=0.0 → window [0.0, 0.5] → top = 0.5
    // user=0.5 → window [0.25, 0.75] → top = 0.75
    // user=1.0 → window [0.5, 1.0] → top = 1.0
    float b0 = compute_brightness(0.0f, 1.0f);
    float b5 = compute_brightness(0.5f, 1.0f);
    float b1 = compute_brightness(1.0f, 1.0f);
    ASSERT_TRUE(b0 > 0.49f && b0 < 0.51f);
    ASSERT_TRUE(b5 > 0.74f && b5 < 0.76f);
    ASSERT_TRUE(b1 > 0.99f && b1 <= 1.0f);
    PASS();
}

TEST(brightness_clamps_inputs) {
    // Out-of-range inputs should be clamped.
    float b1 = compute_brightness(-0.5f, 0.5f);
    float b2 = compute_brightness(0.5f, -0.5f);
    float b3 = compute_brightness(1.5f, 1.5f);
    ASSERT_TRUE(b1 >= 0.0f);
    ASSERT_TRUE(b2 >= 0.0f);
    ASSERT_TRUE(b3 <= 1.0f);
    PASS();
}

TEST(brightness_zero_auto_range) {
    // auto_range=0 → sensor has no effect, user_level maps directly.
    float b = compute_brightness(0.5f, 0.0f, 0.0f, 0.1f, 1.0f);
    float b2 = compute_brightness(0.5f, 1.0f, 0.0f, 0.1f, 1.0f);
    ASSERT_TRUE(b > 0.54f && b < 0.56f);
    // With no auto range, sensor doesn't matter.
    ASSERT_TRUE(b == b2);
    PASS();
}

TEST(brightness_full_auto_range) {
    // auto_range equals total range → window doesn't slide, always [min, max].
    float b_dark = compute_brightness(0.5f, 0.0f, 0.9f, 0.1f, 1.0f);
    float b_bright = compute_brightness(0.5f, 1.0f, 0.9f, 0.1f, 1.0f);
    ASSERT_TRUE(b_dark > 0.09f && b_dark < 0.11f);
    ASSERT_TRUE(b_bright > 0.99f && b_bright <= 1.0f);
    PASS();
}

TEST(brightness_invalid_min_max) {
    ASSERT_TRUE(compute_brightness(0.5f, 0.5f, 0.5f, 1.0f, 0.1f) == 1.0f);
    PASS();
}

// ===========================================================================
// brightness_to_pwm tests
// ===========================================================================

TEST(pwm_min_brightness) {
    ASSERT_EQ(brightness_to_pwm(0.0f), kBacklightMin);
    PASS();
}

TEST(pwm_max_brightness) {
    ASSERT_EQ(brightness_to_pwm(1.0f), kBacklightMax);
    PASS();
}

TEST(pwm_mid_brightness) {
    uint8_t pwm = brightness_to_pwm(0.5f);
    ASSERT_TRUE(pwm > 100 && pwm < 140);
    PASS();
}

TEST(pwm_clamps_below_zero) {
    ASSERT_EQ(brightness_to_pwm(-1.0f), kBacklightMin);
    PASS();
}

TEST(pwm_clamps_above_one) {
    ASSERT_EQ(brightness_to_pwm(2.0f), kBacklightMax);
    PASS();
}

// ===========================================================================
// compute_content_color tests
// ===========================================================================

TEST(content_color_full_bright) {
    ASSERT_EQ(compute_content_color(1.0f), (uint32_t)0xFFFFFF);
    ASSERT_EQ(compute_content_color(2.0f), (uint32_t)0xFFFFFF);
    PASS();
}

TEST(content_color_zero) {
    ASSERT_EQ(compute_content_color(0.0f), (uint32_t)0x1A1A1A);
    ASSERT_EQ(compute_content_color(-1.0f), (uint32_t)0x1A1A1A);
    PASS();
}

TEST(content_color_decreases_with_brightness) {
    uint32_t c_high = compute_content_color(0.8f);
    uint32_t c_mid = compute_content_color(0.5f);
    uint32_t c_low = compute_content_color(0.2f);
    ASSERT_TRUE(c_high > c_mid);
    ASSERT_TRUE(c_mid > c_low);
    ASSERT_TRUE(c_low > (uint32_t)0x1A1A1A);
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
    // With one-shot logic, days_of_week == 0 + enabled is a valid one-shot
    // alarm. It should return 0 when the time matches.
    AlarmTime at{7, 30, 0, true};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), 0);
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
// AlarmTime label tests
// ===========================================================================

TEST(alarm_label_default_empty) {
    AlarmTime at{};
    ASSERT_EQ(at.label[0], '\0');
    PASS();
}

TEST(alarm_set_label_basic) {
    AlarmTime at{};
    alarm_set_label(at, "Work");
    ASSERT_TRUE(strcmp(at.label, "Work") == 0);
    PASS();
}

TEST(alarm_set_label_truncates) {
    AlarmTime at{};
    // 15 chars max (+ null), so a 20-char string gets truncated.
    alarm_set_label(at, "VeryLongLabelName!!");
    ASSERT_EQ(strlen(at.label), alarm_clock::kAlarmLabelMaxLen - 1);
    ASSERT_EQ(at.label[alarm_clock::kAlarmLabelMaxLen - 1], '\0');
    PASS();
}

TEST(alarm_set_label_null) {
    AlarmTime at{};
    alarm_set_label(at, "Work");
    alarm_set_label(at, nullptr);
    ASSERT_EQ(at.label[0], '\0');
    PASS();
}

TEST(alarm_set_label_empty_string) {
    AlarmTime at{};
    alarm_set_label(at, "Work");
    alarm_set_label(at, "");
    ASSERT_EQ(at.label[0], '\0');
    PASS();
}

TEST(alarm_set_label_exact_max) {
    AlarmTime at{};
    // Exactly 15 chars — should fit with null terminator.
    alarm_set_label(at, "123456789012345");
    ASSERT_EQ(strlen(at.label), alarm_clock::kAlarmLabelMaxLen - 1);
    PASS();
}

TEST(alarm_label_max_len_constant) {
    ASSERT_EQ(alarm_clock::kAlarmLabelMaxLen, 16);
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

// ===========================================================================
// Volume ramp tests (compute_ramp_volume from alarmclock.h)
// ===========================================================================

TEST(ramp_volume_at_start) {
    // At t=0, volume should be 10% of configured volume.
    float v = compute_ramp_volume(1.0f, 0);
    ASSERT_TRUE(v > 0.09f && v < 0.11f);
    PASS();
}

TEST(ramp_volume_at_end) {
    // At t=ramp_duration, volume should be configured volume.
    float v = compute_ramp_volume(0.8f, kVolumeRampDurationMs);
    ASSERT_TRUE(v > 0.79f && v < 0.81f);
    PASS();
}

TEST(ramp_volume_past_end) {
    // Past ramp duration, volume should be configured volume.
    float v = compute_ramp_volume(0.6f, kVolumeRampDurationMs + 10000);
    ASSERT_TRUE(v > 0.59f && v < 0.61f);
    PASS();
}

TEST(ramp_volume_midpoint) {
    // At t=half, volume should be ~55% of configured (0.1 + 0.9*0.5 = 0.55).
    float v = compute_ramp_volume(1.0f, kVolumeRampDurationMs / 2);
    ASSERT_TRUE(v > 0.54f && v < 0.56f);
    PASS();
}

TEST(ramp_volume_zero_configured) {
    // Zero configured volume → always zero.
    ASSERT_TRUE(compute_ramp_volume(0.0f, 0) == 0.0f);
    ASSERT_TRUE(compute_ramp_volume(0.0f, 15000) == 0.0f);
    ASSERT_TRUE(compute_ramp_volume(0.0f, kVolumeRampDurationMs) == 0.0f);
    PASS();
}

TEST(ramp_volume_negative_configured) {
    ASSERT_TRUE(compute_ramp_volume(-0.5f, 15000) == 0.0f);
    PASS();
}

TEST(ramp_volume_zero_duration) {
    // Zero ramp duration → immediate full volume.
    float v = compute_ramp_volume(0.7f, 0, 0);
    ASSERT_TRUE(v > 0.69f && v < 0.71f);
    PASS();
}

TEST(ramp_volume_custom_duration) {
    // With a 10-second ramp, at 5 seconds should be ~55% of configured.
    float v = compute_ramp_volume(1.0f, 5000, 10000);
    ASSERT_TRUE(v > 0.54f && v < 0.56f);
    PASS();
}

TEST(ramp_volume_increases_monotonically) {
    // Volume should always increase or stay the same as time progresses.
    float prev = 0.0f;
    for (uint32_t ms = 0; ms <= kVolumeRampDurationMs; ms += 1000) {
        float v = compute_ramp_volume(1.0f, ms);
        ASSERT_TRUE(v >= prev);
        prev = v;
    }
    PASS();
}

TEST(ramp_constants_sanity) {
    ASSERT_EQ(kVolumeRampDurationMs, (uint32_t)30000);
    ASSERT_TRUE(kVolumeRampStartFraction > 0.0f && kVolumeRampStartFraction < 1.0f);
    ASSERT_TRUE(kAlarmPauseDurationMs > 0);
    PASS();
}

// ===========================================================================
// One-shot alarm tests (Task 13)
// ===========================================================================

TEST(is_one_shot_true) {
    AlarmTime at{7, 30, 0, true};
    ASSERT_TRUE(is_one_shot(at));
    PASS();
}

TEST(is_one_shot_false_with_days) {
    AlarmTime at{7, 30, kMonday, true};
    ASSERT_FALSE(is_one_shot(at));
    PASS();
}

TEST(is_one_shot_false_all_days) {
    AlarmTime at{7, 30, kEveryDay, true};
    ASSERT_FALSE(is_one_shot(at));
    PASS();
}

TEST(one_shot_alarm_matches_any_day) {
    AlarmTime at{7, 30, 0, true};
    // One-shot should match on any day of the week.
    for (uint8_t d = 0; d < 7; ++d) {
        ASSERT_TRUE(alarm_clock::alarm_matches(at, 7, 30, d));
    }
    PASS();
}

TEST(one_shot_alarm_no_match_wrong_time) {
    AlarmTime at{7, 30, 0, true};
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 7, 31, 0));
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 8, 30, 0));
    PASS();
}

TEST(one_shot_alarm_no_match_disabled) {
    AlarmTime at{7, 30, 0, false};
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 7, 30, 0));
    PASS();
}

TEST(one_shot_minutes_until_fires_now) {
    AlarmTime at{7, 30, 0, true};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), 0);
    PASS();
}

TEST(one_shot_minutes_until_later_today) {
    AlarmTime at{9, 0, 0, true};
    // 07:30 now, alarm at 09:00 → 90 minutes.
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 3), 90);
    PASS();
}

TEST(one_shot_minutes_until_tomorrow) {
    AlarmTime at{6, 0, 0, true};
    // 22:00 now, alarm at 06:00 → tomorrow.
    // 24*60 - 22*60 + 6*60 = 1440 - 1320 + 360 = 480
    int32_t expected = 24 * 60 - time_to_minutes(22, 0) + time_to_minutes(6, 0);
    ASSERT_EQ(minutes_until_alarm(at, 22, 0, 5), expected);
    PASS();
}

TEST(one_shot_minutes_until_disabled) {
    AlarmTime at{7, 30, 0, false};
    ASSERT_EQ(minutes_until_alarm(at, 7, 30, 1), -1);
    PASS();
}

// ===========================================================================
// find_next_alarm_index tests (Task 15)
// ===========================================================================

TEST(find_next_alarm_none_enabled) {
    AlarmTime alarms[4] = {};
    // All disabled.
    int32_t minutes = 0;
    ASSERT_EQ(find_next_alarm_index(alarms, 4, 7, 0, 1, &minutes), -1);
    PASS();
}

TEST(find_next_alarm_single_enabled) {
    AlarmTime alarms[4] = {};
    alarms[0] = {7, 30, kWeekdays, true};
    int32_t minutes = 0;
    int8_t idx = find_next_alarm_index(alarms, 4, 7, 0, 1, &minutes);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(minutes, 30);
    PASS();
}

TEST(find_next_alarm_picks_nearest) {
    AlarmTime alarms[4] = {};
    alarms[0] = {9, 0, kWeekdays, true};
    alarms[1] = {7, 30, kWeekdays, true};
    alarms[2] = {12, 0, kWeekdays, true};
    int32_t minutes = 0;
    int8_t idx = find_next_alarm_index(alarms, 4, 7, 0, 1, &minutes);
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(minutes, 30);
    PASS();
}

TEST(find_next_alarm_skips_disabled) {
    AlarmTime alarms[4] = {};
    alarms[0] = {7, 30, kWeekdays, false};  // disabled
    alarms[1] = {9, 0, kWeekdays, true};
    int32_t minutes = 0;
    int8_t idx = find_next_alarm_index(alarms, 4, 7, 0, 1, &minutes);
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(minutes, 120);
    PASS();
}

TEST(find_next_alarm_one_shot) {
    AlarmTime alarms[4] = {};
    alarms[0] = {8, 0, 0, true};  // one-shot
    int32_t minutes = 0;
    int8_t idx = find_next_alarm_index(alarms, 4, 7, 30, 3, &minutes);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(minutes, 30);
    PASS();
}

TEST(find_next_alarm_null_minutes) {
    AlarmTime alarms[4] = {};
    alarms[0] = {8, 0, kEveryDay, true};
    int8_t idx = find_next_alarm_index(alarms, 4, 7, 0, 1, nullptr);
    ASSERT_EQ(idx, 0);
    PASS();
}

TEST(find_next_alarm_fires_now_is_zero) {
    AlarmTime alarms[4] = {};
    alarms[0] = {7, 30, kMonday, true};
    int32_t minutes = -1;
    int8_t idx = find_next_alarm_index(alarms, 4, 7, 30, 1, &minutes);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(minutes, 0);
    PASS();
}

// ===========================================================================
// format_next_alarm_text tests (Task 15)
// ===========================================================================

TEST(format_next_alarm_with_label_hours) {
    AlarmTime at{7, 0, kWeekdays, true};
    alarm_set_label(at, "Work");
    char buf[64];
    format_next_alarm_text(at, 390, buf, sizeof(buf));
    // 390 min = 6h 30m → "7:00 AM — Work (in 6h 30m)"
    ASSERT_TRUE(strstr(buf, "7:00 AM") != nullptr);
    ASSERT_TRUE(strstr(buf, "Work") != nullptr);
    ASSERT_TRUE(strstr(buf, "6h 30m") != nullptr);
    PASS();
}

TEST(format_next_alarm_with_label_minutes_only) {
    AlarmTime at{7, 30, kWeekdays, true};
    alarm_set_label(at, "Work");
    char buf[64];
    format_next_alarm_text(at, 45, buf, sizeof(buf));
    // 45 min → "7:30 AM — Work (in 45m)"
    ASSERT_TRUE(strstr(buf, "7:30 AM") != nullptr);
    ASSERT_TRUE(strstr(buf, "Work") != nullptr);
    ASSERT_TRUE(strstr(buf, "45m") != nullptr);
    // Should NOT have "h" in it.
    ASSERT_TRUE(strstr(buf, "h ") == nullptr);
    PASS();
}

TEST(format_next_alarm_no_label_hours) {
    AlarmTime at{14, 0, kEveryDay, true};
    char buf[64];
    format_next_alarm_text(at, 120, buf, sizeof(buf));
    // 14:00 → 2:00 PM; 120 min = 2h 0m
    ASSERT_TRUE(strstr(buf, "2:00 PM") != nullptr);
    ASSERT_TRUE(strstr(buf, "2h 0m") != nullptr);
    PASS();
}

TEST(format_next_alarm_no_label_minutes_only) {
    AlarmTime at{0, 30, kEveryDay, true};
    char buf[64];
    format_next_alarm_text(at, 15, buf, sizeof(buf));
    // 0:30 → 12:30 AM; 15 min
    ASSERT_TRUE(strstr(buf, "12:30 AM") != nullptr);
    ASSERT_TRUE(strstr(buf, "15m") != nullptr);
    PASS();
}

TEST(format_next_alarm_null_buf) {
    AlarmTime at{7, 0, kWeekdays, true};
    ASSERT_EQ(format_next_alarm_text(at, 60, nullptr, 64), (size_t)0);
    PASS();
}

TEST(format_next_alarm_zero_buf_size) {
    AlarmTime at{7, 0, kWeekdays, true};
    char buf[1];
    ASSERT_EQ(format_next_alarm_text(at, 60, buf, 0), (size_t)0);
    PASS();
}

TEST(format_next_alarm_pm_time) {
    AlarmTime at{23, 45, kWeekends, true};
    alarm_set_label(at, "Late");
    char buf[64];
    format_next_alarm_text(at, 60, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "11:45 PM") != nullptr);
    ASSERT_TRUE(strstr(buf, "Late") != nullptr);
    PASS();
}

// ===========================================================================
// format_pre_alarm_text tests (Task 10)
// ===========================================================================

TEST(format_pre_alarm_with_label) {
    char buf[48];
    format_pre_alarm_text(5, "Work", buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "Alarm in 5 min") != nullptr);
    ASSERT_TRUE(strstr(buf, "Work") != nullptr);
    PASS();
}

TEST(format_pre_alarm_no_label) {
    char buf[48];
    format_pre_alarm_text(3, nullptr, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "Alarm in 3 min") != nullptr);
    PASS();
}

TEST(format_pre_alarm_empty_label) {
    char buf[48];
    format_pre_alarm_text(1, "", buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "Alarm in 1 min") != nullptr);
    PASS();
}

TEST(format_pre_alarm_null_buf) {
    ASSERT_EQ(format_pre_alarm_text(5, "Work", nullptr, 48), (size_t)0);
    PASS();
}

TEST(format_pre_alarm_zero_buf_size) {
    char buf[1];
    ASSERT_EQ(format_pre_alarm_text(5, "Work", buf, 0), (size_t)0);
    PASS();
}

TEST(pre_alarm_constant) {
    ASSERT_EQ(kPreAlarmMinutes, 5);
    PASS();
}

// ===========================================================================
// 12h/24h time conversion tests (Task 3)
// ===========================================================================

TEST(hour_24_to_12_midnight) {
    auto [h, pm] = hour_24_to_12(0);
    ASSERT_EQ(h, 12);
    ASSERT_FALSE(pm);
    PASS();
}

TEST(hour_24_to_12_noon) {
    auto [h, pm] = hour_24_to_12(12);
    ASSERT_EQ(h, 12);
    ASSERT_TRUE(pm);
    PASS();
}

TEST(hour_24_to_12_morning) {
    auto [h1, pm1] = hour_24_to_12(1);
    ASSERT_EQ(h1, 1);
    ASSERT_FALSE(pm1);

    auto [h9, pm9] = hour_24_to_12(9);
    ASSERT_EQ(h9, 9);
    ASSERT_FALSE(pm9);

    auto [h11, pm11] = hour_24_to_12(11);
    ASSERT_EQ(h11, 11);
    ASSERT_FALSE(pm11);
    PASS();
}

TEST(hour_24_to_12_afternoon) {
    auto [h13, pm13] = hour_24_to_12(13);
    ASSERT_EQ(h13, 1);
    ASSERT_TRUE(pm13);

    auto [h17, pm17] = hour_24_to_12(17);
    ASSERT_EQ(h17, 5);
    ASSERT_TRUE(pm17);

    auto [h23, pm23] = hour_24_to_12(23);
    ASSERT_EQ(h23, 11);
    ASSERT_TRUE(pm23);
    PASS();
}

TEST(hour_12_to_24_midnight) {
    ASSERT_EQ(hour_12_to_24(12, false), 0);
    PASS();
}

TEST(hour_12_to_24_noon) {
    ASSERT_EQ(hour_12_to_24(12, true), 12);
    PASS();
}

TEST(hour_12_to_24_am) {
    ASSERT_EQ(hour_12_to_24(1, false), 1);
    ASSERT_EQ(hour_12_to_24(6, false), 6);
    ASSERT_EQ(hour_12_to_24(11, false), 11);
    PASS();
}

TEST(hour_12_to_24_pm) {
    ASSERT_EQ(hour_12_to_24(1, true), 13);
    ASSERT_EQ(hour_12_to_24(6, true), 18);
    ASSERT_EQ(hour_12_to_24(11, true), 23);
    PASS();
}

TEST(hour_roundtrip_all) {
    // Converting 24h → 12h → 24h should be identity for all valid hours.
    for (uint8_t h = 0; h < 24; ++h) {
        auto [h12, pm] = hour_24_to_12(h);
        uint8_t back = hour_12_to_24(h12, pm);
        ASSERT_EQ(back, h);
    }
    PASS();
}

// ===========================================================================
// Auto-dismiss timer tests (Task 11)
// ===========================================================================

TEST(auto_dismiss_constant) {
    ASSERT_EQ(kAutoDismissMinutes, (uint16_t)30);
    PASS();
}

TEST(tick_firing_noop_when_idle) {
    AlarmStateMachine sm;
    ASSERT_FALSE(sm.tick_firing());
    ASSERT_EQ(sm.firing_elapsed_minutes(), 0);
    PASS();
}

TEST(tick_firing_noop_when_snoozed) {
    AlarmStateMachine sm;
    sm.trigger();
    sm.snooze();
    ASSERT_FALSE(sm.tick_firing());
    PASS();
}

TEST(tick_firing_increments) {
    AlarmStateMachine sm;
    sm.trigger();
    ASSERT_FALSE(sm.tick_firing());
    ASSERT_EQ(sm.firing_elapsed_minutes(), 1);
    ASSERT_FALSE(sm.tick_firing());
    ASSERT_EQ(sm.firing_elapsed_minutes(), 2);
    PASS();
}

TEST(tick_firing_auto_dismiss_at_30) {
    AlarmStateMachine sm;
    sm.trigger();
    // Tick 29 times — should not auto-dismiss.
    for (uint16_t i = 0; i < 29; ++i) {
        ASSERT_FALSE(sm.tick_firing());
    }
    ASSERT_EQ(sm.firing_elapsed_minutes(), 29);
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kFiring));
    // 30th tick — auto-dismiss.
    ASSERT_TRUE(sm.tick_firing());
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kIdle));
    ASSERT_EQ(sm.firing_elapsed_minutes(), 0);
    PASS();
}

TEST(tick_firing_resets_on_trigger) {
    AlarmStateMachine sm;
    sm.trigger();
    sm.tick_firing();
    sm.tick_firing();
    ASSERT_EQ(sm.firing_elapsed_minutes(), 2);
    sm.dismiss();
    sm.trigger();
    ASSERT_EQ(sm.firing_elapsed_minutes(), 0);
    PASS();
}

TEST(tick_firing_resets_after_snooze_expires) {
    AlarmStateMachine sm;
    sm.set_snooze_duration(2);
    sm.trigger();
    // Tick firing a few times.
    sm.tick_firing();
    sm.tick_firing();
    ASSERT_EQ(sm.firing_elapsed_minutes(), 2);
    // Snooze.
    sm.snooze();
    // Snooze expires.
    sm.tick_snooze();
    sm.tick_snooze();
    ASSERT_EQ(static_cast<uint8_t>(sm.state()), static_cast<uint8_t>(AlarmState::kFiring));
    // Firing elapsed should have been reset when snooze expired.
    ASSERT_EQ(sm.firing_elapsed_minutes(), 0);
    PASS();
}

TEST(tick_firing_resets_on_reset) {
    AlarmStateMachine sm;
    sm.trigger();
    sm.tick_firing();
    sm.tick_firing();
    sm.reset();
    ASSERT_EQ(sm.firing_elapsed_minutes(), 0);
    PASS();
}

// ===========================================================================
// Storage serialization tests (Task 5)
// ===========================================================================

TEST(serialized_alarm_size_constant) {
    ASSERT_EQ(alarm_clock::kSerializedAlarmSize, (size_t)21);
    PASS();
}

TEST(serialized_settings_size_constant) {
    ASSERT_EQ(alarm_clock::kSerializedSettingsSize, (size_t)12);
    PASS();
}

TEST(serialize_alarm_roundtrip) {
    AlarmTime orig{};
    orig.hour = 7;
    orig.minute = 30;
    orig.days_of_week = kWeekdays;
    orig.enabled = true;
    alarm_set_label(orig, "Work");

    uint8_t buf[alarm_clock::kSerializedAlarmSize];
    size_t written = alarm_clock::serialize_alarm(orig, buf, sizeof(buf));
    ASSERT_EQ(written, alarm_clock::kSerializedAlarmSize);

    AlarmTime loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_alarm(buf, written, &loaded));
    ASSERT_EQ(loaded.hour, 7);
    ASSERT_EQ(loaded.minute, 30);
    ASSERT_EQ(loaded.days_of_week, kWeekdays);
    ASSERT_TRUE(loaded.enabled);
    ASSERT_TRUE(strcmp(loaded.label, "Work") == 0);
    PASS();
}

TEST(serialize_alarm_disabled) {
    AlarmTime orig{};
    orig.hour = 22;
    orig.minute = 15;
    orig.days_of_week = kWeekends;
    orig.enabled = false;
    alarm_set_label(orig, "Weekend");

    uint8_t buf[alarm_clock::kSerializedAlarmSize];
    size_t written = alarm_clock::serialize_alarm(orig, buf, sizeof(buf));
    ASSERT_EQ(written, alarm_clock::kSerializedAlarmSize);

    AlarmTime loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_alarm(buf, written, &loaded));
    ASSERT_EQ(loaded.hour, 22);
    ASSERT_EQ(loaded.minute, 15);
    ASSERT_EQ(loaded.days_of_week, kWeekends);
    ASSERT_FALSE(loaded.enabled);
    ASSERT_TRUE(strcmp(loaded.label, "Weekend") == 0);
    PASS();
}

TEST(serialize_alarm_empty_label) {
    AlarmTime orig{};
    orig.hour = 6;
    orig.minute = 0;
    orig.days_of_week = kEveryDay;
    orig.enabled = true;

    uint8_t buf[alarm_clock::kSerializedAlarmSize];
    alarm_clock::serialize_alarm(orig, buf, sizeof(buf));

    AlarmTime loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_alarm(buf, sizeof(buf), &loaded));
    ASSERT_EQ(loaded.label[0], '\0');
    PASS();
}

TEST(serialize_alarm_one_shot) {
    AlarmTime orig{};
    orig.hour = 14;
    orig.minute = 45;
    orig.days_of_week = 0;  // one-shot
    orig.enabled = true;
    alarm_set_label(orig, "Nap");

    uint8_t buf[alarm_clock::kSerializedAlarmSize];
    alarm_clock::serialize_alarm(orig, buf, sizeof(buf));

    AlarmTime loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_alarm(buf, sizeof(buf), &loaded));
    ASSERT_EQ(loaded.days_of_week, 0);
    ASSERT_TRUE(is_one_shot(loaded));
    ASSERT_TRUE(strcmp(loaded.label, "Nap") == 0);
    PASS();
}

TEST(serialize_alarm_max_label) {
    AlarmTime orig{};
    orig.hour = 8;
    orig.minute = 0;
    orig.days_of_week = kMonday;
    orig.enabled = true;
    alarm_set_label(orig, "123456789012345");  // exactly 15 chars

    uint8_t buf[alarm_clock::kSerializedAlarmSize];
    alarm_clock::serialize_alarm(orig, buf, sizeof(buf));

    AlarmTime loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_alarm(buf, sizeof(buf), &loaded));
    ASSERT_EQ(strlen(loaded.label), (size_t)15);
    ASSERT_TRUE(strcmp(loaded.label, "123456789012345") == 0);
    PASS();
}

TEST(serialize_alarm_null_buf) {
    AlarmTime alarm{};
    ASSERT_EQ(alarm_clock::serialize_alarm(alarm, nullptr, 100), (size_t)0);
    PASS();
}

TEST(serialize_alarm_small_buf) {
    AlarmTime alarm{};
    uint8_t buf[5];
    ASSERT_EQ(alarm_clock::serialize_alarm(alarm, buf, sizeof(buf)), (size_t)0);
    PASS();
}

TEST(deserialize_alarm_null_buf) {
    AlarmTime alarm{};
    ASSERT_FALSE(alarm_clock::deserialize_alarm(nullptr, 100, &alarm));
    PASS();
}

TEST(deserialize_alarm_null_alarm) {
    uint8_t buf[alarm_clock::kSerializedAlarmSize] = {};
    ASSERT_FALSE(alarm_clock::deserialize_alarm(buf, sizeof(buf), nullptr));
    PASS();
}

TEST(deserialize_alarm_small_buf) {
    AlarmTime alarm{};
    uint8_t buf[5] = {};
    ASSERT_FALSE(alarm_clock::deserialize_alarm(buf, sizeof(buf), &alarm));
    PASS();
}

TEST(deserialize_alarm_wrong_version) {
    uint8_t buf[alarm_clock::kSerializedAlarmSize] = {};
    buf[0] = 99;  // wrong version
    AlarmTime alarm{};
    ASSERT_FALSE(alarm_clock::deserialize_alarm(buf, sizeof(buf), &alarm));
    PASS();
}

TEST(serialize_settings_roundtrip) {
    alarm_clock::StorageSettings orig{};
    orig.volume = 0.75f;
    orig.brightness = 0.3f;
    orig.snooze_duration_minutes = 15;
    orig.time_format_24h = true;
    orig.selected_sound_index = 2;

    uint8_t buf[alarm_clock::kSerializedSettingsSize];
    size_t written = alarm_clock::serialize_settings(orig, buf, sizeof(buf));
    ASSERT_EQ(written, alarm_clock::kSerializedSettingsSize);

    alarm_clock::StorageSettings loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_settings(buf, written, &loaded));
    ASSERT_TRUE(loaded.volume > 0.74f && loaded.volume < 0.76f);
    ASSERT_TRUE(loaded.brightness > 0.29f && loaded.brightness < 0.31f);
    ASSERT_EQ(loaded.snooze_duration_minutes, 15);
    ASSERT_TRUE(loaded.time_format_24h);
    ASSERT_EQ(loaded.selected_sound_index, 2);
    PASS();
}

TEST(serialize_settings_defaults) {
    alarm_clock::StorageSettings orig{};  // all defaults

    uint8_t buf[alarm_clock::kSerializedSettingsSize];
    alarm_clock::serialize_settings(orig, buf, sizeof(buf));

    alarm_clock::StorageSettings loaded{};
    ASSERT_TRUE(alarm_clock::deserialize_settings(buf, sizeof(buf), &loaded));
    ASSERT_TRUE(loaded.volume > 0.49f && loaded.volume < 0.51f);
    ASSERT_TRUE(loaded.brightness > 0.49f && loaded.brightness < 0.51f);
    ASSERT_EQ(loaded.snooze_duration_minutes, 9);
    ASSERT_FALSE(loaded.time_format_24h);
    ASSERT_EQ(loaded.selected_sound_index, 0);
    PASS();
}

TEST(serialize_settings_null_buf) {
    alarm_clock::StorageSettings s{};
    ASSERT_EQ(alarm_clock::serialize_settings(s, nullptr, 100), (size_t)0);
    PASS();
}

TEST(serialize_settings_small_buf) {
    alarm_clock::StorageSettings s{};
    uint8_t buf[5];
    ASSERT_EQ(alarm_clock::serialize_settings(s, buf, sizeof(buf)), (size_t)0);
    PASS();
}

TEST(deserialize_settings_null_buf) {
    alarm_clock::StorageSettings s{};
    ASSERT_FALSE(alarm_clock::deserialize_settings(nullptr, 100, &s));
    PASS();
}

TEST(deserialize_settings_null_settings) {
    uint8_t buf[alarm_clock::kSerializedSettingsSize] = {};
    ASSERT_FALSE(alarm_clock::deserialize_settings(buf, sizeof(buf), nullptr));
    PASS();
}

TEST(deserialize_settings_small_buf) {
    alarm_clock::StorageSettings s{};
    uint8_t buf[5] = {};
    ASSERT_FALSE(alarm_clock::deserialize_settings(buf, sizeof(buf), &s));
    PASS();
}

TEST(deserialize_settings_wrong_version) {
    uint8_t buf[alarm_clock::kSerializedSettingsSize] = {};
    buf[0] = 99;
    alarm_clock::StorageSettings s{};
    ASSERT_FALSE(alarm_clock::deserialize_settings(buf, sizeof(buf), &s));
    PASS();
}

TEST(serialize_alarm_version_byte) {
    AlarmTime alarm{};
    alarm.hour = 12;
    alarm.minute = 0;
    uint8_t buf[alarm_clock::kSerializedAlarmSize];
    alarm_clock::serialize_alarm(alarm, buf, sizeof(buf));
    ASSERT_EQ(buf[0], alarm_clock::kStorageVersion);
    PASS();
}

TEST(serialize_settings_version_byte) {
    alarm_clock::StorageSettings s{};
    uint8_t buf[alarm_clock::kSerializedSettingsSize];
    alarm_clock::serialize_settings(s, buf, sizeof(buf));
    ASSERT_EQ(buf[0], alarm_clock::kStorageVersion);
    PASS();
}

// ===========================================================================
// Alarm sound selection tests (Task 4)
// ===========================================================================

TEST(alarm_sound_count) {
    ASSERT_EQ(kAlarmSoundCount, (uint8_t)5);
    PASS();
}

TEST(get_alarm_sound_rtttl_valid) {
    // Each valid index should return a non-null, non-empty string.
    for (uint8_t i = 0; i < kAlarmSoundCount; ++i) {
        const char *rtttl = get_alarm_sound_rtttl(i);
        ASSERT_TRUE(rtttl != nullptr);
        ASSERT_TRUE(strlen(rtttl) > 0);
    }
    PASS();
}

TEST(get_alarm_sound_rtttl_out_of_range) {
    // Out of range should clamp to index 0.
    const char *r = get_alarm_sound_rtttl(kAlarmSoundCount);
    ASSERT_TRUE(r == kAlarmSounds[0].rtttl);
    const char *r2 = get_alarm_sound_rtttl(255);
    ASSERT_TRUE(r2 == kAlarmSounds[0].rtttl);
    PASS();
}

TEST(get_alarm_sound_name_valid) {
    for (uint8_t i = 0; i < kAlarmSoundCount; ++i) {
        const char *name = get_alarm_sound_name(i);
        ASSERT_TRUE(name != nullptr);
        ASSERT_TRUE(strlen(name) > 0);
    }
    PASS();
}

TEST(get_alarm_sound_name_out_of_range) {
    const char *n = get_alarm_sound_name(kAlarmSoundCount);
    ASSERT_TRUE(n == kAlarmSounds[0].name);
    PASS();
}

// ===========================================================================
// Snooze duration option tests (Task 7)
// ===========================================================================

TEST(snooze_option_to_minutes_valid) {
    ASSERT_EQ(snooze_option_to_minutes(0), (uint8_t)5);
    ASSERT_EQ(snooze_option_to_minutes(1), (uint8_t)9);
    ASSERT_EQ(snooze_option_to_minutes(2), (uint8_t)10);
    ASSERT_EQ(snooze_option_to_minutes(3), (uint8_t)15);
    PASS();
}

TEST(snooze_option_to_minutes_out_of_range) {
    // Out of range defaults to 9 min.
    ASSERT_EQ(snooze_option_to_minutes(4), (uint8_t)9);
    ASSERT_EQ(snooze_option_to_minutes(255), (uint8_t)9);
    PASS();
}

TEST(snooze_minutes_to_option_valid) {
    ASSERT_EQ(snooze_minutes_to_option(5), (uint8_t)0);
    ASSERT_EQ(snooze_minutes_to_option(9), (uint8_t)1);
    ASSERT_EQ(snooze_minutes_to_option(10), (uint8_t)2);
    ASSERT_EQ(snooze_minutes_to_option(15), (uint8_t)3);
    PASS();
}

TEST(snooze_minutes_to_option_not_found) {
    // Unknown duration defaults to index 1 (9 min).
    ASSERT_EQ(snooze_minutes_to_option(7), (uint8_t)1);
    ASSERT_EQ(snooze_minutes_to_option(0), (uint8_t)1);
    ASSERT_EQ(snooze_minutes_to_option(20), (uint8_t)1);
    PASS();
}

TEST(snooze_duration_options_constant) {
    ASSERT_EQ(kSnoozeDurationOptionCount, (uint8_t)4);
    ASSERT_EQ(kSnoozeDurationOptions[0], (uint8_t)5);
    ASSERT_EQ(kSnoozeDurationOptions[1], (uint8_t)9);
    ASSERT_EQ(kSnoozeDurationOptions[2], (uint8_t)10);
    ASSERT_EQ(kSnoozeDurationOptions[3], (uint8_t)15);
    PASS();
}

// ===========================================================================
// format_clock_time tests (Task 9)
// ===========================================================================

TEST(format_clock_time_12h_am) {
    char buf[16];
    format_clock_time(7, 30, false, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "7:30 AM") != nullptr);
    PASS();
}

TEST(format_clock_time_12h_pm) {
    char buf[16];
    format_clock_time(14, 5, false, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "2:05 PM") != nullptr);
    PASS();
}

TEST(format_clock_time_12h_midnight) {
    char buf[16];
    format_clock_time(0, 0, false, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "12:00 AM") != nullptr);
    PASS();
}

TEST(format_clock_time_12h_noon) {
    char buf[16];
    format_clock_time(12, 0, false, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "12:00 PM") != nullptr);
    PASS();
}

TEST(format_clock_time_24h) {
    char buf[16];
    format_clock_time(14, 5, true, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "14:05") == 0);
    PASS();
}

TEST(format_clock_time_24h_midnight) {
    char buf[16];
    format_clock_time(0, 0, true, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "00:00") == 0);
    PASS();
}

TEST(format_clock_time_null_buf) {
    ASSERT_EQ(format_clock_time(7, 30, false, nullptr, 16), (size_t)0);
    PASS();
}

TEST(format_clock_time_zero_buf_size) {
    char buf[1];
    ASSERT_EQ(format_clock_time(7, 30, false, buf, 0), (size_t)0);
    PASS();
}

// ===========================================================================
// Day-of-week conversion tests (Batch 1, Issue #1/#2)
// Validates that 0-based day indices (as converted from ESPTime's 1-based
// day_of_week) produce correct results through the alarm logic.
// ===========================================================================

TEST(espttime_dow_conversion_sunday) {
    // ESPTime: day_of_week=1 (Sunday) → 0-based index = 0
    uint8_t dow = 1 - 1;  // Simulates the YAML fix
    ASSERT_EQ(dow, 0);
    ASSERT_EQ(day_index_to_flag(dow), kSunday);
    PASS();
}

TEST(espttime_dow_conversion_saturday) {
    // ESPTime: day_of_week=7 (Saturday) → 0-based index = 6
    uint8_t dow = 7 - 1;  // Simulates the YAML fix
    ASSERT_EQ(dow, 6);
    ASSERT_EQ(day_index_to_flag(dow), kSaturday);
    PASS();
}

TEST(espttime_dow_conversion_monday_alarm) {
    // ESPTime: day_of_week=2 (Monday) → 0-based index = 1
    uint8_t dow = 2 - 1;
    AlarmTime at{7, 30, kMonday, true};
    ASSERT_TRUE(alarm_clock::alarm_matches(at, 7, 30, dow));
    PASS();
}

TEST(espttime_dow_alarm_does_not_fire_wrong_day) {
    // Alarm set for Monday only. ESPTime says Tuesday (day_of_week=3 → index 2).
    uint8_t dow = 3 - 1;
    AlarmTime at{7, 30, kMonday, true};
    ASSERT_FALSE(alarm_clock::alarm_matches(at, 7, 30, dow));
    PASS();
}

TEST(espttime_dow_all_days_roundtrip) {
    // Verify all 7 days convert correctly from ESPTime 1-based to our 0-based.
    // ESPTime: 1=Sun, 2=Mon, 3=Tue, 4=Wed, 5=Thu, 6=Fri, 7=Sat
    uint8_t expected_flags[] = {kSunday, kMonday, kTuesday, kWednesday,
                                kThursday, kFriday, kSaturday};
    for (uint8_t esp_dow = 1; esp_dow <= 7; ++esp_dow) {
        uint8_t idx = esp_dow - 1;
        ASSERT_EQ(day_index_to_flag(idx), expected_flags[idx]);
    }
    PASS();
}

TEST(one_shot_alarm_should_be_visible) {
    // One-shot alarms (days_of_week==0, enabled) should be shown in UI sync.
    // This tests the condition: enabled || days_of_week != 0 || hour != 0 || minute != 0
    AlarmTime at{14, 30, 0, true};
    ASSERT_TRUE(at.enabled);
    ASSERT_TRUE(is_one_shot(at));
    // The sync_ui_ condition should include this alarm.
    bool should_show = (at.enabled || at.days_of_week != 0 ||
                        at.hour != 0 || at.minute != 0);
    ASSERT_TRUE(should_show);
    PASS();
}

TEST(default_alarm_not_one_shot) {
    // Default alarm at 7:00 weekdays should not be one-shot.
    AlarmTime at{7, 0, kWeekdays, true};
    ASSERT_FALSE(is_one_shot(at));
    // Should also be visible.
    bool should_show = (at.enabled || at.days_of_week != 0 ||
                        at.hour != 0 || at.minute != 0);
    ASSERT_TRUE(should_show);
    PASS();
}

TEST(empty_alarm_not_shown) {
    // A completely default alarm slot should not be shown.
    AlarmTime at{};  // hour=0, minute=0, days=0, enabled=false
    bool should_show = (at.enabled || at.days_of_week != 0 ||
                        at.hour != 0 || at.minute != 0);
    ASSERT_FALSE(should_show);
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

    // lux_to_sensor_factor
    RUN(sensor_factor_dark);
    RUN(sensor_factor_bright);
    RUN(sensor_factor_midpoint);
    RUN(sensor_factor_invalid_range);

    // compute_brightness
    RUN(brightness_user_high_sensor_bright);
    RUN(brightness_user_high_sensor_dark);
    RUN(brightness_user_low_sensor_bright);
    RUN(brightness_user_low_sensor_dark);
    RUN(brightness_user_mid_sensor_mid);
    RUN(brightness_no_sensor);
    RUN(brightness_clamps_inputs);
    RUN(brightness_zero_auto_range);
    RUN(brightness_full_auto_range);
    RUN(brightness_invalid_min_max);

    // brightness_to_pwm
    RUN(pwm_min_brightness);
    RUN(pwm_max_brightness);
    RUN(pwm_mid_brightness);
    RUN(pwm_clamps_below_zero);
    RUN(pwm_clamps_above_one);

    // compute_content_color
    RUN(content_color_full_bright);
    RUN(content_color_zero);
    RUN(content_color_decreases_with_brightness);

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

    // AlarmTime labels
    RUN(alarm_label_default_empty);
    RUN(alarm_set_label_basic);
    RUN(alarm_set_label_truncates);
    RUN(alarm_set_label_null);
    RUN(alarm_set_label_empty_string);
    RUN(alarm_set_label_exact_max);
    RUN(alarm_label_max_len_constant);

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

    // Volume ramp
    RUN(ramp_volume_at_start);
    RUN(ramp_volume_at_end);
    RUN(ramp_volume_past_end);
    RUN(ramp_volume_midpoint);
    RUN(ramp_volume_zero_configured);
    RUN(ramp_volume_negative_configured);
    RUN(ramp_volume_zero_duration);
    RUN(ramp_volume_custom_duration);
    RUN(ramp_volume_increases_monotonically);
    RUN(ramp_constants_sanity);

    // One-shot alarm (Task 13)
    RUN(is_one_shot_true);
    RUN(is_one_shot_false_with_days);
    RUN(is_one_shot_false_all_days);
    RUN(one_shot_alarm_matches_any_day);
    RUN(one_shot_alarm_no_match_wrong_time);
    RUN(one_shot_alarm_no_match_disabled);
    RUN(one_shot_minutes_until_fires_now);
    RUN(one_shot_minutes_until_later_today);
    RUN(one_shot_minutes_until_tomorrow);
    RUN(one_shot_minutes_until_disabled);

    // find_next_alarm_index (Task 15)
    RUN(find_next_alarm_none_enabled);
    RUN(find_next_alarm_single_enabled);
    RUN(find_next_alarm_picks_nearest);
    RUN(find_next_alarm_skips_disabled);
    RUN(find_next_alarm_one_shot);
    RUN(find_next_alarm_null_minutes);
    RUN(find_next_alarm_fires_now_is_zero);

    // format_next_alarm_text (Task 15)
    RUN(format_next_alarm_with_label_hours);
    RUN(format_next_alarm_with_label_minutes_only);
    RUN(format_next_alarm_no_label_hours);
    RUN(format_next_alarm_no_label_minutes_only);
    RUN(format_next_alarm_null_buf);
    RUN(format_next_alarm_zero_buf_size);
    RUN(format_next_alarm_pm_time);

    // format_pre_alarm_text (Task 10)
    RUN(format_pre_alarm_with_label);
    RUN(format_pre_alarm_no_label);
    RUN(format_pre_alarm_empty_label);
    RUN(format_pre_alarm_null_buf);
    RUN(format_pre_alarm_zero_buf_size);
    RUN(pre_alarm_constant);

    // 12h/24h time conversion (Task 3)
    RUN(hour_24_to_12_midnight);
    RUN(hour_24_to_12_noon);
    RUN(hour_24_to_12_morning);
    RUN(hour_24_to_12_afternoon);
    RUN(hour_12_to_24_midnight);
    RUN(hour_12_to_24_noon);
    RUN(hour_12_to_24_am);
    RUN(hour_12_to_24_pm);
    RUN(hour_roundtrip_all);

    // Auto-dismiss timer (Task 11)
    RUN(auto_dismiss_constant);
    RUN(tick_firing_noop_when_idle);
    RUN(tick_firing_noop_when_snoozed);
    RUN(tick_firing_increments);
    RUN(tick_firing_auto_dismiss_at_30);
    RUN(tick_firing_resets_on_trigger);
    RUN(tick_firing_resets_after_snooze_expires);
    RUN(tick_firing_resets_on_reset);

    // Storage serialization (Task 5)
    RUN(serialized_alarm_size_constant);
    RUN(serialized_settings_size_constant);
    RUN(serialize_alarm_roundtrip);
    RUN(serialize_alarm_disabled);
    RUN(serialize_alarm_empty_label);
    RUN(serialize_alarm_one_shot);
    RUN(serialize_alarm_max_label);
    RUN(serialize_alarm_null_buf);
    RUN(serialize_alarm_small_buf);
    RUN(deserialize_alarm_null_buf);
    RUN(deserialize_alarm_null_alarm);
    RUN(deserialize_alarm_small_buf);
    RUN(deserialize_alarm_wrong_version);
    RUN(serialize_settings_roundtrip);
    RUN(serialize_settings_defaults);
    RUN(serialize_settings_null_buf);
    RUN(serialize_settings_small_buf);
    RUN(deserialize_settings_null_buf);
    RUN(deserialize_settings_null_settings);
    RUN(deserialize_settings_small_buf);
    RUN(deserialize_settings_wrong_version);
    RUN(serialize_alarm_version_byte);
    RUN(serialize_settings_version_byte);

    // Alarm sound selection (Task 4)
    RUN(alarm_sound_count);
    RUN(get_alarm_sound_rtttl_valid);
    RUN(get_alarm_sound_rtttl_out_of_range);
    RUN(get_alarm_sound_name_valid);
    RUN(get_alarm_sound_name_out_of_range);

    // Snooze duration options (Task 7)
    RUN(snooze_option_to_minutes_valid);
    RUN(snooze_option_to_minutes_out_of_range);
    RUN(snooze_minutes_to_option_valid);
    RUN(snooze_minutes_to_option_not_found);
    RUN(snooze_duration_options_constant);

    // format_clock_time (Task 9)
    RUN(format_clock_time_12h_am);
    RUN(format_clock_time_12h_pm);
    RUN(format_clock_time_12h_midnight);
    RUN(format_clock_time_12h_noon);
    RUN(format_clock_time_24h);
    RUN(format_clock_time_24h_midnight);
    RUN(format_clock_time_null_buf);
    RUN(format_clock_time_zero_buf_size);

    // Day-of-week conversion (Batch 1, Issue #1/#2)
    RUN(espttime_dow_conversion_sunday);
    RUN(espttime_dow_conversion_saturday);
    RUN(espttime_dow_conversion_monday_alarm);
    RUN(espttime_dow_alarm_does_not_fire_wrong_day);
    RUN(espttime_dow_all_days_roundtrip);

    // One-shot alarm visibility (Batch 1, Issue #6)
    RUN(one_shot_alarm_should_be_visible);
    RUN(default_alarm_not_one_shot);
    RUN(empty_alarm_not_shown);

    printf("\n%d test(s) run, %d failed.\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
