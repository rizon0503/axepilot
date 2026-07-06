#include <unity.h>
#include "core/ControlsScreen.h"
#include "core/Limits.h"

void setUp(void) {}
void tearDown(void) {}

using ControlsScreen::Action;
using ControlsScreen::Rect;

static int centerX(const Rect& r) { return r.x + r.w / 2; }
static int centerY(const Rect& r) { return r.y + r.h / 2; }

void test_hit_test_returns_preset_actions() {
    TEST_ASSERT_EQUAL(Action::PRESET_ECO, ControlsScreen::hitTest(centerX(ControlsScreen::PRESET_ECO_RECT), centerY(ControlsScreen::PRESET_ECO_RECT)));
    TEST_ASSERT_EQUAL(Action::PRESET_BALANCED, ControlsScreen::hitTest(centerX(ControlsScreen::PRESET_BALANCED_RECT), centerY(ControlsScreen::PRESET_BALANCED_RECT)));
    TEST_ASSERT_EQUAL(Action::PRESET_TURBO, ControlsScreen::hitTest(centerX(ControlsScreen::PRESET_TURBO_RECT), centerY(ControlsScreen::PRESET_TURBO_RECT)));
    TEST_ASSERT_EQUAL(Action::PRESET_MAX, ControlsScreen::hitTest(centerX(ControlsScreen::PRESET_MAX_RECT), centerY(ControlsScreen::PRESET_MAX_RECT)));
}

void test_hit_test_returns_toggle_and_restart() {
    TEST_ASSERT_EQUAL(Action::TOGGLE_MODE, ControlsScreen::hitTest(centerX(ControlsScreen::TOGGLE_MODE_RECT), centerY(ControlsScreen::TOGGLE_MODE_RECT)));
    TEST_ASSERT_EQUAL(Action::RESTART, ControlsScreen::hitTest(centerX(ControlsScreen::RESTART_RECT), centerY(ControlsScreen::RESTART_RECT)));
}

void test_hit_test_returns_switch_screen_for_tab() {
    TEST_ASSERT_EQUAL(Action::SWITCH_SCREEN, ControlsScreen::hitTest(centerX(ControlsScreen::TAB_RECT), centerY(ControlsScreen::TAB_RECT)));
}

void test_hit_test_returns_none_outside_all_buttons() {
    TEST_ASSERT_EQUAL(Action::NONE, ControlsScreen::hitTest(5, 5)); // above the first row
    TEST_ASSERT_EQUAL(Action::NONE, ControlsScreen::hitTest(0, 239));
}

void test_hit_test_main_screen_returns_switch_screen_for_tab_only() {
    TEST_ASSERT_EQUAL(Action::SWITCH_SCREEN, ControlsScreen::hitTestMainScreen(centerX(ControlsScreen::TAB_RECT), centerY(ControlsScreen::TAB_RECT)));
    TEST_ASSERT_EQUAL(Action::NONE, ControlsScreen::hitTestMainScreen(160, 200)); // inside the emergency throttle rect, not the tab
}

void test_preset_for_returns_valid_settings_within_limits() {
    struct { Action action; } presetActions[] = {
        {Action::PRESET_ECO}, {Action::PRESET_BALANCED}, {Action::PRESET_TURBO}, {Action::PRESET_MAX}
    };
    for (auto& entry : presetActions) {
        ControlsScreen::Preset p = ControlsScreen::presetFor(entry.action);
        TEST_ASSERT_TRUE(Limits::isValidSetting(p.frequency, p.coreVoltage));
    }
}

void test_preset_for_returns_zero_for_non_preset_actions() {
    ControlsScreen::Preset none = ControlsScreen::presetFor(Action::NONE);
    TEST_ASSERT_EQUAL(0, none.frequency);
    TEST_ASSERT_EQUAL(0, none.coreVoltage);

    ControlsScreen::Preset toggle = ControlsScreen::presetFor(Action::TOGGLE_MODE);
    TEST_ASSERT_EQUAL(0, toggle.frequency);
    TEST_ASSERT_EQUAL(0, toggle.coreVoltage);
}

void test_preset_for_extremes_match_eco_and_max_limits() {
    ControlsScreen::Preset eco = ControlsScreen::presetFor(Action::PRESET_ECO);
    TEST_ASSERT_EQUAL(Limits::FREQ_MIN, eco.frequency);
    TEST_ASSERT_EQUAL(Limits::VOLT_MIN, eco.coreVoltage);

    ControlsScreen::Preset max = ControlsScreen::presetFor(Action::PRESET_MAX);
    TEST_ASSERT_EQUAL(Limits::FREQ_MAX, max.frequency);
    TEST_ASSERT_EQUAL(Limits::VOLT_MAX, max.coreVoltage);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_hit_test_returns_preset_actions);
    RUN_TEST(test_hit_test_returns_toggle_and_restart);
    RUN_TEST(test_hit_test_returns_switch_screen_for_tab);
    RUN_TEST(test_hit_test_returns_none_outside_all_buttons);
    RUN_TEST(test_hit_test_main_screen_returns_switch_screen_for_tab_only);
    RUN_TEST(test_preset_for_returns_valid_settings_within_limits);
    RUN_TEST(test_preset_for_returns_zero_for_non_preset_actions);
    RUN_TEST(test_preset_for_extremes_match_eco_and_max_limits);
    return UNITY_END();
}
