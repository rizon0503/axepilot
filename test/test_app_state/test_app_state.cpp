#include <unity.h>
#include "core/AppState.h"

void setUp(void) {}
void tearDown(void) {}

using AppState::RestoreTimer;
using AppState::ScreenPower;

// --- ScreenPower ---

void test_screen_starts_on() {
    ScreenPower power;
    TEST_ASSERT_TRUE(power.isOn());
}

void test_touch_while_on_and_past_debounce_is_active() {
    ScreenPower power;
    bool active = power.onTouch(true, 1000);
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_FALSE(power.justWoke());
}

void test_no_touch_leaves_state_unchanged() {
    ScreenPower power;
    TEST_ASSERT_FALSE(power.onTouch(false, 1000));
    TEST_ASSERT_TRUE(power.isOn());
    // Idle clock wasn't reset by a real touch, so it should still time out
    // from t=0, not from t=1000.
    TEST_ASSERT_TRUE(power.tick(15001));
}

void test_auto_off_fires_once_after_idle_timeout() {
    ScreenPower power; // 15000ms default timeout
    power.onTouch(true, 0); // establish a last-interaction baseline

    TEST_ASSERT_FALSE(power.tick(14999));
    TEST_ASSERT_TRUE(power.isOn());

    TEST_ASSERT_TRUE(power.tick(15001));
    TEST_ASSERT_FALSE(power.isOn());

    // Doesn't keep firing every subsequent tick once already off.
    TEST_ASSERT_FALSE(power.tick(20000));
}

void test_touch_resets_the_idle_timer() {
    ScreenPower power;
    power.onTouch(true, 0);
    power.onTouch(true, 10000); // interaction again before timeout

    TEST_ASSERT_FALSE(power.tick(15001)); // only 5001ms since the real reset
    TEST_ASSERT_TRUE(power.isOn());
}

void test_wake_from_off_suppresses_touch_and_turns_screen_on() {
    ScreenPower power;
    power.onTouch(true, 0);
    power.tick(20000); // idle timeout -> screen off
    TEST_ASSERT_FALSE(power.isOn());

    bool active = power.onTouch(true, 20500);
    TEST_ASSERT_FALSE(active); // this tap only wakes the screen
    TEST_ASSERT_TRUE(power.isOn());
    TEST_ASSERT_TRUE(power.justWoke());
}

void test_debounce_window_suppresses_touch_after_wake() {
    ScreenPower power(15000, 300);
    power.onTouch(true, 0);
    power.tick(20000);
    power.onTouch(true, 20500); // wakes; debounce window now [20500, 20800]

    bool active = power.onTouch(true, 20700); // still within the window
    TEST_ASSERT_FALSE(active);
    TEST_ASSERT_FALSE(power.justWoke()); // this tick isn't the wake tick
}

void test_touch_after_debounce_window_is_active() {
    ScreenPower power(15000, 300);
    power.onTouch(true, 0);
    power.tick(20000);
    power.onTouch(true, 20500); // wakes; debounce window now [20500, 20800]

    bool active = power.onTouch(true, 20801); // just past the window
    TEST_ASSERT_TRUE(active);
}

// --- RestoreTimer ---

void test_restore_timer_not_pending_initially() {
    RestoreTimer timer;
    TEST_ASSERT_FALSE(timer.isPending());
    TEST_ASSERT_FALSE(timer.tick(0));
}

void test_restore_timer_pending_after_trigger() {
    RestoreTimer timer;
    timer.trigger(1000, 2000);
    TEST_ASSERT_TRUE(timer.isPending());
}

void test_restore_timer_does_not_fire_before_duration_elapses() {
    RestoreTimer timer;
    timer.trigger(1000, 2000);
    TEST_ASSERT_FALSE(timer.tick(2999));
    TEST_ASSERT_TRUE(timer.isPending());
}

void test_restore_timer_fires_once_when_duration_elapses() {
    RestoreTimer timer;
    timer.trigger(1000, 2000);
    TEST_ASSERT_TRUE(timer.tick(3000));
    TEST_ASSERT_FALSE(timer.isPending());

    // Doesn't keep firing on subsequent ticks.
    TEST_ASSERT_FALSE(timer.tick(5000));
}

void test_restore_timer_can_be_retriggered() {
    RestoreTimer timer;
    timer.trigger(1000, 2000);
    timer.tick(3000); // fires and clears

    timer.trigger(10000, 500);
    TEST_ASSERT_TRUE(timer.isPending());
    TEST_ASSERT_FALSE(timer.tick(10499));
    TEST_ASSERT_TRUE(timer.tick(10500));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_screen_starts_on);
    RUN_TEST(test_touch_while_on_and_past_debounce_is_active);
    RUN_TEST(test_no_touch_leaves_state_unchanged);
    RUN_TEST(test_auto_off_fires_once_after_idle_timeout);
    RUN_TEST(test_touch_resets_the_idle_timer);
    RUN_TEST(test_wake_from_off_suppresses_touch_and_turns_screen_on);
    RUN_TEST(test_debounce_window_suppresses_touch_after_wake);
    RUN_TEST(test_touch_after_debounce_window_is_active);
    RUN_TEST(test_restore_timer_not_pending_initially);
    RUN_TEST(test_restore_timer_pending_after_trigger);
    RUN_TEST(test_restore_timer_does_not_fire_before_duration_elapses);
    RUN_TEST(test_restore_timer_fires_once_when_duration_elapses);
    RUN_TEST(test_restore_timer_can_be_retriggered);
    return UNITY_END();
}
