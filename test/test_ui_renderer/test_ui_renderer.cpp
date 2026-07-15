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
    d.errorPercentage = 1.2f;
    return d;
}

// Most tests don't care about the sparkline history — a valid (non-null),
// unused-content pointer with count 0.
static const float kNoHistory[1] = {0.0f};

void test_render_telemetry_draws_all_fields() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL_STRING("Temp: 60.5 C", display.lastTextAt(10, 8)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Hashrate: 950.0 GH/s", display.lastTextAt(10, 33)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("V:1150mV F:550MHz", display.lastTextAt(10, 58)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Mode: AUTO", display.lastTextAt(10, 83)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Err: 1.2%", display.lastTextAt(10, 130)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::GREEN, display.lastTextAt(10, 130)->color);
    TEST_ASSERT_EQUAL_STRING("Pow: 15.2W Fan: 4200rpm", display.lastTextAt(10, 156)->text.c_str());
}

void test_render_telemetry_shows_overheating_color() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.isOverheating = true;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL(Colors::RED, display.lastTextAt(10, 8)->color);
}

void test_render_telemetry_uses_th_s_above_9999_ghs() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.hashrate = 12345.0f;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL_STRING("Hashrate: 12.3 TH/s", display.lastTextAt(10, 33)->text.c_str());
}

void test_render_telemetry_shows_fan_percent_when_manual() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.fanSpeedPercent = 80;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL_STRING("Pow: 15.2W Fan: 80%", display.lastTextAt(10, 156)->text.c_str());
}

void test_render_telemetry_shows_error_rate_red_above_threshold() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.errorPercentage = 5.1f;

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL_STRING("Err: 5.1%", display.lastTextAt(10, 130)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::RED, display.lastTextAt(10, 130)->color);
}

void test_render_telemetry_shows_error_rate_green_at_threshold() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();
    data.errorPercentage = 5.0f; // exactly at the threshold: still green

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL(Colors::GREEN, display.lastTextAt(10, 130)->color);
}

void test_render_telemetry_shows_wifi_reconnecting_instead_of_mode() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, false, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL_STRING("Wi-Fi reconnecting...", display.lastTextAt(10, 83)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::RED, display.lastTextAt(10, 83)->color);
}

void test_render_telemetry_shows_manual_mode_in_orange() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderTelemetry(sampleData(), OperationMode::MANUAL, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL_STRING("Mode: MANUAL", display.lastTextAt(10, 83)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::ORANGE, display.lastTextAt(10, 83)->color);
}

void test_render_telemetry_skips_unchanged_lines() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);
    size_t callsAfterFirst = display.textCalls.size();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0); // identical data again
    TEST_ASSERT_EQUAL(callsAfterFirst, display.textCalls.size()); // no new draws
}

void test_render_telemetry_redraws_line_that_changed() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);
    size_t callsAfterFirst = display.textCalls.size();

    data.temperature = 61.0f; // only temp changes
    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);

    TEST_ASSERT_EQUAL(callsAfterFirst + 1, display.textCalls.size());
    TEST_ASSERT_EQUAL_STRING("Temp: 61.0 C", display.lastTextAt(10, 8)->text.c_str());
}

void test_reset_telemetry_cache_forces_full_repaint() {
    MockDisplay display;
    UiRenderer renderer(display);
    BitaxeData data = sampleData();

    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0);
    size_t callsAfterFirst = display.textCalls.size();

    renderer.resetTelemetryCache();
    renderer.renderTelemetry(data, OperationMode::AUTOPILOT, true, kNoHistory, 0, kNoHistory, 0); // same data, but cache was reset

    TEST_ASSERT_EQUAL(callsAfterFirst * 2, display.textCalls.size());
}

void test_render_telemetry_draws_sparkline_for_history() {
    MockDisplay display;
    UiRenderer renderer(display);
    float temps[] = {58.0f, 60.0f, 59.0f, 61.0f};
    float hashes[] = {900.0f, 920.0f, 910.0f, 950.0f};

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 4, hashes, 4);

    // 4 points -> 3 line segments per sparkline
    TEST_ASSERT_EQUAL(6, display.lineCalls.size());
    // Clears each sparkline's region before drawing (dirty-checked, but this is the first call)
    TEST_ASSERT_EQUAL(2, display.fillRectCalls.size());
}

void test_render_telemetry_sparkline_skipped_when_history_unchanged() {
    MockDisplay display;
    UiRenderer renderer(display);
    float temps[] = {58.0f, 60.0f, 59.0f};
    float hashes[] = {900.0f, 920.0f, 910.0f};

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, hashes, 3);
    size_t linesAfterFirst = display.lineCalls.size();
    size_t fillsAfterFirst = display.fillRectCalls.size();

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, hashes, 3); // identical history

    TEST_ASSERT_EQUAL(linesAfterFirst, display.lineCalls.size());
    TEST_ASSERT_EQUAL(fillsAfterFirst, display.fillRectCalls.size());
}

void test_render_telemetry_sparkline_redraws_when_history_changes() {
    MockDisplay display;
    UiRenderer renderer(display);
    float temps[] = {58.0f, 60.0f, 59.0f};
    float hashes[] = {900.0f, 920.0f, 910.0f};

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, hashes, 3);
    size_t fillsAfterFirst = display.fillRectCalls.size();

    temps[2] = 65.0f; // a new temperature sample came in
    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, hashes, 3);

    // Only the temp sparkline's region should be re-cleared; hashrate is unchanged.
    TEST_ASSERT_EQUAL(fillsAfterFirst + 1, display.fillRectCalls.size());
}

void test_render_telemetry_sparkline_does_nothing_with_fewer_than_two_points() {
    MockDisplay display;
    UiRenderer renderer(display);
    float temps[] = {60.0f};

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 1, temps, 1);

    // Still clears the (now-empty) sparkline regions, but draws no line segments.
    TEST_ASSERT_EQUAL(0, display.lineCalls.size());
}

void test_reset_telemetry_cache_forces_sparkline_repaint() {
    MockDisplay display;
    UiRenderer renderer(display);
    float temps[] = {58.0f, 60.0f, 59.0f};

    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, temps, 3);
    size_t fillsAfterFirst = display.fillRectCalls.size();

    renderer.resetTelemetryCache();
    renderer.renderTelemetry(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, temps, 3); // identical history, but cache was reset

    TEST_ASSERT_EQUAL(fillsAfterFirst * 2, display.fillRectCalls.size());
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

void test_render_main_screen_chrome_draws_sparkline_labels() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderMainScreenChrome();

    // "T"/"H" letters (#64) so the color-only distinction between the two
    // sparklines isn't the only way to tell them apart.
    TEST_ASSERT_EQUAL_STRING("T", display.lastTextAt(10, 106)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::GREEN, display.lastTextAt(10, 106)->color);
    TEST_ASSERT_EQUAL_STRING("H", display.lastTextAt(170, 106)->text.c_str());
    TEST_ASSERT_EQUAL(Colors::WHITE, display.lastTextAt(170, 106)->color);
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

void test_render_ota_screen_clears_and_shows_title() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderOtaScreen();

    TEST_ASSERT_EQUAL(1, display.clearCalls);
    TEST_ASSERT_EQUAL_STRING("OTA update...", display.lastTextAt(10, 40)->text.c_str());
    TEST_ASSERT_EQUAL_STRING("Do not power off", display.lastTextAt(10, 70)->text.c_str());
}

void test_render_ota_progress_draws_percent_and_bar() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderOtaProgress(40);

    TEST_ASSERT_EQUAL_STRING("40%", display.lastTextAt(10, 110)->text.c_str());
    TEST_ASSERT_EQUAL(1, display.fillRectCalls.size());
    const auto& bar = display.fillRectCalls[0];
    TEST_ASSERT_EQUAL(10, bar.x);
    TEST_ASSERT_EQUAL(150, bar.y);
    TEST_ASSERT_EQUAL(120, bar.w); // 40% of the 300px-wide bar
    TEST_ASSERT_EQUAL(20, bar.h);
    TEST_ASSERT_EQUAL(Colors::GREEN, bar.color);
}

void test_render_ota_progress_clamps_out_of_range() {
    MockDisplay display;
    UiRenderer renderer(display);

    renderer.renderOtaProgress(150);

    TEST_ASSERT_EQUAL_STRING("100%", display.lastTextAt(10, 110)->text.c_str());
    TEST_ASSERT_EQUAL(300, display.fillRectCalls[0].w); // full bar, not 450px

    renderer.renderOtaProgress(-5);
    TEST_ASSERT_EQUAL_STRING("0%", display.lastTextAt(10, 110)->text.c_str());
    TEST_ASSERT_EQUAL(0, display.fillRectCalls[1].w);
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
    RUN_TEST(test_render_telemetry_shows_error_rate_red_above_threshold);
    RUN_TEST(test_render_telemetry_shows_error_rate_green_at_threshold);
    RUN_TEST(test_render_telemetry_uses_th_s_above_9999_ghs);
    RUN_TEST(test_render_telemetry_shows_fan_percent_when_manual);
    RUN_TEST(test_render_telemetry_shows_wifi_reconnecting_instead_of_mode);
    RUN_TEST(test_render_telemetry_shows_manual_mode_in_orange);
    RUN_TEST(test_render_telemetry_skips_unchanged_lines);
    RUN_TEST(test_render_telemetry_redraws_line_that_changed);
    RUN_TEST(test_reset_telemetry_cache_forces_full_repaint);
    RUN_TEST(test_render_telemetry_draws_sparkline_for_history);
    RUN_TEST(test_render_telemetry_sparkline_skipped_when_history_unchanged);
    RUN_TEST(test_render_telemetry_sparkline_redraws_when_history_changes);
    RUN_TEST(test_render_telemetry_sparkline_does_nothing_with_fewer_than_two_points);
    RUN_TEST(test_reset_telemetry_cache_forces_sparkline_repaint);
    RUN_TEST(test_render_throttle_button_normal_state);
    RUN_TEST(test_render_throttle_button_triggered_state);
    RUN_TEST(test_render_main_screen_chrome_clears_and_draws_throttle_and_tab);
    RUN_TEST(test_render_main_screen_chrome_draws_sparkline_labels);
    RUN_TEST(test_render_controls_screen_shows_auto_mode);
    RUN_TEST(test_render_controls_screen_shows_manual_mode);
    RUN_TEST(test_render_restart_button_states);
    RUN_TEST(test_render_ota_screen_clears_and_shows_title);
    RUN_TEST(test_render_ota_progress_draws_percent_and_bar);
    RUN_TEST(test_render_ota_progress_clamps_out_of_range);
    RUN_TEST(test_render_diagnostics_screen_shows_all_fields);
    return UNITY_END();
}
