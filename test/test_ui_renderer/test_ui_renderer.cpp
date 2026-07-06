#include <unity.h>
#include "core/UiRenderer.h"
#include "core/ControlsScreen.h"
#include "../mocks/MockDisplay.h"

void setUp(void) {}
void tearDown(void) {}

// Mirrors TFT_eSPI's TFT_* macros (RGB565) without depending on the
// hardware-only TFT_eSPI header — see UiRenderer.cpp for the same values.
namespace Colors {
constexpr uint16_t RED = 0xF800;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t ORANGE = 0xFDA0;
constexpr uint16_t BLUE = 0x001F;
} // namespace Colors

static BitaxeData sampleData() {
    BitaxeData d;
    d.temperature = 60.5f;
    d.hashrate = 950.0f;
    d.coreVoltage = 1150;
    d.frequency = 550;
    d.power = 15.2f;
    d.fanRpm = 4200;
    d.fanSpeedPercent = 0; // autofan: shown as RPM, not percent
    return d;
}

void test_render_telemetry_draws_all_fields() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true);

    TEST_ASSERT_EQUAL_STRING("Temp: 60.5 C", display.lastTextAt(10, 10)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Hashrate: 950.0 GH/s", display.lastTextAt(10, 40)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Volt: 1150 mV", display.lastTextAt(10, 70)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Freq: 550 MHz", display.lastTextAt(10, 100)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Pow: 15.2W Fan: 4200rpm", display.lastTextAt(10, 155)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Mode: AUTO", display.lastTextAt(10, 129)->text.c_str());
}

void test_render_telemetry_shows_overheating_color() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.isOverheating = true;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);

    TEST_ASSERT_EQUAL(Colors::RED, display.lastTextAt(10, 10)->color);
}

void test_render_telemetry_uses_th_s_above_9999_ghs() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.hashrate = 12345.0f;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);

    TEST_ASSERT_EQUAL_STRING("Hashrate: 12.3 TH/s", display.lastTextAt(10, 40)->text.c_str());
}

void test_render_telemetry_shows_fan_percent_when_manual() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.fanSpeedPercent = 80;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);

    TEST_ASSERT_EQUAL_STRING("Pow: 15.2W Fan: 80%", display.lastTextAt(10, 155)->text.c_str());
}

void test_render_telemetry_shows_wifi_reconnecting_instead_of_mode() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, false);

    TEST_ASSERT_EQUAL_STRING("Wi-Fi reconnecting...", display.lastTextAt(10, 129)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::RED, display.lastTextAt(10, 129)->color);
}

void test_render_telemetry_shows_manual_mode_in_orange() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderTelemetry(sampleData(), OperationMode::MANUAL, true);

    TEST_ASSERT_EQUAL_STRING("Mode: MANUAL", display.lastTextAt(10, 129)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::ORANGE, display.lastTextAt(10, 129)->color);
}

void test_render_telemetry_skips_unchanged_lines() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);
    size_t callsAfterFirst = display.textCalls.size();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true); // identical data again
    TEST_ASSERT_EQUAL(callsAfterFirst, display.textCalls.size()); // no new draws
}

void test_render_telemetry_redraws_line_that_changed() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);
    size_t callsAfterFirst = display.textCalls.size();

    data.temperature = 61.0f; // only temp changes
    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);

    TEST_ASSERT_EQUAL(callsAfterFirst + 1, display.textCalls.size());
    TEST_ASSERT_EQUAL_STRING("Temp: 61.0 C", display.lastTextAt(10, 10)->text.c_str());
}

void test_reset_telemetry_cache_forces_full_repaint() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true);
    size_t callsAfterFirst = display.textCalls.size();

    renderer.resetTelemetryCache();
    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true); // same data, but cache was reset

    TEST_ASSERT_EQUAL(callsAfterFirst * 2, display.textCalls.size());
}

void test_render_throttle_button_normal_state() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderThrottleButton(UiRenderer::ThrottleState::NORMAL);

    const auto* btn = display.lastButtonAt(0, 180);
    TEST_ASSERT_EQUAL_STRING("EMERGENCY THROTTLE", btn->label.c_str());
    TEST_ASSERT_EQUAL(Colors::RED, btn->color);
}

void test_render_throttle_button_triggered_state() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderThrottleButton(UiRenderer::ThrottleState::TRIGGERED);

    const auto* btn = display.lastButtonAt(0, 180);
    TEST_ASSERT_EQUAL_STRING("THROTTLED!", btn->label.c_str());
    TEST_ASSERT_EQUAL(Colors::ORANGE, btn->color);
}

void test_render_main_screen_chrome_clears_and_draws_throttle_and_tab() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderMainScreenChrome();

    TEST_ASSERT_EQUAL(1, display.clearCalls);
    TEST_ASSERT_EQUAL_STRING("EMERGENCY THROTTLE", display.lastButtonAt(0, 180)->label.c_str());
    TEST_ASSERT_EQUAL_STRING("CTRL", display.lastButtonAt(ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y)->label.c_str());
}

void test_render_controls_screen_shows_auto_mode() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderControlsScreen(OperationMode::AUTOPILOT);

    TEST_ASSERT_EQUAL(1, display.clearCalls);
    const auto* toggle = display.lastButtonAt(ControlsScreen::TOGGLE_MODE_RECT.x, ControlsScreen::TOGGLE_MODE_RECT.y);
    TEST_ASSERT_EQUAL_STRING("AUTO", toggle->label.c_str());
    TEST_ASSERT_EQUAL(Colors::GREEN, toggle->color);
    TEST_ASSERT_EQUAL_STRING("DIAG", display.lastButtonAt(ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y)->label.c_str());
}

void test_render_controls_screen_shows_manual_mode() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderControlsScreen(OperationMode::MANUAL);

    const auto* toggle = display.lastButtonAt(ControlsScreen::TOGGLE_MODE_RECT.x, ControlsScreen::TOGGLE_MODE_RECT.y);
    TEST_ASSERT_EQUAL_STRING("MANUAL", toggle->label.c_str());
    TEST_ASSERT_EQUAL(Colors::ORANGE, toggle->color);
}

void test_render_restart_button_states() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderRestartButton(UiRenderer::RestartButtonState::SENT);
    const auto* sent = display.lastButtonAt(ControlsScreen::RESTART_RECT.x, ControlsScreen::RESTART_RECT.y);
    TEST_ASSERT_EQUAL_STRING("SENT", sent->label.c_str());
    TEST_ASSERT_EQUAL(Colors::ORANGE, sent->color);

    renderer.renderRestartButton(UiRenderer::RestartButtonState::NORMAL);
    const auto* normal = display.lastButtonAt(ControlsScreen::RESTART_RECT.x, ControlsScreen::RESTART_RECT.y);
    TEST_ASSERT_EQUAL_STRING("RESTART", normal->label.c_str());
    TEST_ASSERT_EQUAL(Colors::RED, normal->color);
}

void test_render_diagnostics_screen_shows_all_fields() {
    MockDisplay display;
    UiRenderer renderer(display);

    UiRenderer::DiagnosticsData diag;
    diag.uptimeSeconds = 3661; // 1h 01m 01s
    diag.resetReason = "POWERON";
    diag.resetCount = 7;
    diag.wifiRssi = -55;
    diag.wifiReconnectCount = 2;
    diag.freeHeapBytes = 150 * 1024;
    diag.minFreeHeapBytes = 120 * 1024;
    diag.maxAllocBytes = 100 * 1024;
    diag.interventionTotal = 42;

    renderer.renderDiagnosticsScreen(diag);

    TEST_ASSERT_EQUAL(1, display.clearCalls);
    TEST_ASSERT_EQUAL_STRING("Uptime: 01:01:01", display.lastTextAt(10, 40)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Reset: POWERON (x7)", display.lastTextAt(10, 70)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("WiFi: -55 dBm (drops: 2)", display.lastTextAt(10, 100)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Heap: 150 KB (min 120 KB)", display.lastTextAt(10, 130)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Max alloc: 100 KB", display.lastTextAt(10, 160)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Interventions: 42", display.lastTextAt(10, 190)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("BACK", display.lastButtonAt(ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y)->label.c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_render_telemetry_draws_all_fields);
    RUN_TEST(test_render_telemetry_shows_overheating_color);
    RUN_TEST(test_render_telemetry_uses_th_s_above_9999_ghs);
    RUN_TEST(test_render_telemetry_shows_fan_percent_when_manual);
    RUN_TEST(test_render_telemetry_shows_wifi_reconnecting_instead_of_mode);
    RUN_TEST(test_render_telemetry_shows_manual_mode_in_orange);
    RUN_TEST(test_render_telemetry_skips_unchanged_lines);
    RUN_TEST(test_render_telemetry_redraws_line_that_changed);
    RUN_TEST(test_reset_telemetry_cache_forces_full_repaint);
    RUN_TEST(test_render_throttle_button_normal_state);
    RUN_TEST(test_render_throttle_button_triggered_state);
    RUN_TEST(test_render_main_screen_chrome_clears_and_draws_throttle_and_tab);
    RUN_TEST(test_render_controls_screen_shows_auto_mode);
    RUN_TEST(test_render_controls_screen_shows_manual_mode);
    RUN_TEST(test_render_restart_button_states);
    RUN_TEST(test_render_diagnostics_screen_shows_all_fields);
    return UNITY_END();
}
