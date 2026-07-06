#include "core/UiRenderer.h"
#include "core/ControlsScreen.h"
#include <cstdio>
#include <cstring>

// Mirrors TFT_eSPI's TFT_* macros (RGB565) without depending on the
// hardware-only TFT_eSPI header, keeping this file natively testable.
namespace {
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;
constexpr uint16_t COLOR_CYAN = 0x07FF;
constexpr uint16_t COLOR_ORANGE = 0xFDA0;
constexpr uint16_t COLOR_BLUE = 0x001F;

// The emergency throttle button's rect isn't defined as a shared constant
// anywhere (unlike ControlsScreen's rects) — main.cpp's touch hit-test
// duplicates these same four numbers, matching pre-existing behavior.
constexpr int THROTTLE_X = 0, THROTTLE_Y = 180, THROTTLE_W = 320, THROTTLE_H = 60;
} // namespace

UiRenderer::UiRenderer(IDisplay& display) : display(display) {}

void UiRenderer::drawIfChanged(int x, int y, const char* text, uint16_t color, LineCache& cache) {
    if (cache.color == color && strncmp(cache.text, text, sizeof(cache.text)) == 0) {
        return;
    }
    display.drawText(x, y, text, color);
    snprintf(cache.text, sizeof(cache.text), "%s", text);
    cache.color = color;
}

void UiRenderer::renderTelemetry(const BitaxeData& data, OperationMode mode, bool wifiOk) {
    char buf[64];

    snprintf(buf, sizeof(buf), "Temp: %.1f C", data.temperature);
    drawIfChanged(10, 10, buf, data.isOverheating ? COLOR_RED : COLOR_GREEN, tempCache_);

    // If hashrate is crazy high (like 86000 GH/s), display it in TH/s to save screen space
    if (data.hashrate > 9999.0f) {
        snprintf(buf, sizeof(buf), "Hashrate: %.1f TH/s", data.hashrate / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "Hashrate: %.1f GH/s", data.hashrate);
    }
    drawIfChanged(10, 40, buf, COLOR_WHITE, hashrateCache_);

    snprintf(buf, sizeof(buf), "Volt: %d mV", data.coreVoltage);
    drawIfChanged(10, 70, buf, COLOR_YELLOW, voltCache_);

    snprintf(buf, sizeof(buf), "Freq: %d MHz", data.frequency);
    drawIfChanged(10, 100, buf, COLOR_CYAN, freqCache_);

    // In autofanspeed mode AxeOS reports fanspeed=0%, so RPM is the
    // meaningful number; show the percent only when it is set manually.
    if (data.fanSpeedPercent > 0) {
        snprintf(buf, sizeof(buf), "Pow: %.1fW Fan: %d%%", data.power, data.fanSpeedPercent);
    } else {
        snprintf(buf, sizeof(buf), "Pow: %.1fW Fan: %drpm", data.power, data.fanRpm);
    }
    drawIfChanged(10, 155, buf, COLOR_WHITE, powCache_);

    // y=129 (not 130): Font 4 is 26px tall, and the row below sits at
    // y=155 — at y=130 this row's fillRect would clip one pixel off the
    // top of that row's text.
    if (!wifiOk) {
        drawIfChanged(10, 129, "Wi-Fi reconnecting...", COLOR_RED, modeCache_);
    } else {
        snprintf(buf, sizeof(buf), "Mode: %s", mode == OperationMode::AUTOPILOT ? "AUTO" : "MANUAL");
        drawIfChanged(10, 129, buf, mode == OperationMode::AUTOPILOT ? COLOR_GREEN : COLOR_ORANGE, modeCache_);
    }
}

void UiRenderer::resetTelemetryCache() {
    tempCache_.text[0] = '\0';
    hashrateCache_.text[0] = '\0';
    voltCache_.text[0] = '\0';
    freqCache_.text[0] = '\0';
    powCache_.text[0] = '\0';
    modeCache_.text[0] = '\0';
}

void UiRenderer::renderThrottleButton(ThrottleState state) {
    if (state == ThrottleState::TRIGGERED) {
        display.drawButton(THROTTLE_X, THROTTLE_Y, THROTTLE_W, THROTTLE_H, "THROTTLED!", COLOR_ORANGE);
    } else {
        display.drawButton(THROTTLE_X, THROTTLE_Y, THROTTLE_W, THROTTLE_H, "EMERGENCY THROTTLE", COLOR_RED);
    }
}

void UiRenderer::renderMainScreenChrome() {
    display.clear();
    renderThrottleButton(ThrottleState::NORMAL);
    display.drawButton(ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y,
                        ControlsScreen::TAB_RECT.w, ControlsScreen::TAB_RECT.h, "CTRL", COLOR_BLUE);
}

void UiRenderer::renderControlsScreen(OperationMode mode) {
    display.clear();
    display.drawButton(ControlsScreen::PRESET_ECO_RECT.x, ControlsScreen::PRESET_ECO_RECT.y,
                        ControlsScreen::PRESET_ECO_RECT.w, ControlsScreen::PRESET_ECO_RECT.h, "400 MHz", COLOR_CYAN);
    display.drawButton(ControlsScreen::PRESET_BALANCED_RECT.x, ControlsScreen::PRESET_BALANCED_RECT.y,
                        ControlsScreen::PRESET_BALANCED_RECT.w, ControlsScreen::PRESET_BALANCED_RECT.h, "500 MHz", COLOR_CYAN);
    display.drawButton(ControlsScreen::PRESET_TURBO_RECT.x, ControlsScreen::PRESET_TURBO_RECT.y,
                        ControlsScreen::PRESET_TURBO_RECT.w, ControlsScreen::PRESET_TURBO_RECT.h, "575 MHz", COLOR_CYAN);
    display.drawButton(ControlsScreen::PRESET_MAX_RECT.x, ControlsScreen::PRESET_MAX_RECT.y,
                        ControlsScreen::PRESET_MAX_RECT.w, ControlsScreen::PRESET_MAX_RECT.h, "625 MHz", COLOR_CYAN);

    bool isAuto = mode == OperationMode::AUTOPILOT;
    display.drawButton(ControlsScreen::TOGGLE_MODE_RECT.x, ControlsScreen::TOGGLE_MODE_RECT.y,
                        ControlsScreen::TOGGLE_MODE_RECT.w, ControlsScreen::TOGGLE_MODE_RECT.h,
                        isAuto ? "AUTO" : "MANUAL", isAuto ? COLOR_GREEN : COLOR_ORANGE);
    renderRestartButton(RestartButtonState::NORMAL);
    display.drawButton(ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y,
                        ControlsScreen::TAB_RECT.w, ControlsScreen::TAB_RECT.h, "DIAG", COLOR_BLUE);
}

void UiRenderer::renderRestartButton(RestartButtonState state) {
    if (state == RestartButtonState::SENT) {
        display.drawButton(ControlsScreen::RESTART_RECT.x, ControlsScreen::RESTART_RECT.y,
                            ControlsScreen::RESTART_RECT.w, ControlsScreen::RESTART_RECT.h, "SENT", COLOR_ORANGE);
    } else {
        display.drawButton(ControlsScreen::RESTART_RECT.x, ControlsScreen::RESTART_RECT.y,
                            ControlsScreen::RESTART_RECT.w, ControlsScreen::RESTART_RECT.h, "RESTART", COLOR_RED);
    }
}

void UiRenderer::renderDiagnosticsScreen(const DiagnosticsData& diag) {
    display.clear();
    char buf[64];

    snprintf(buf, sizeof(buf), "Uptime: %02u:%02u:%02u",
             (unsigned)(diag.uptimeSeconds / 3600), (unsigned)((diag.uptimeSeconds % 3600) / 60), (unsigned)(diag.uptimeSeconds % 60));
    display.drawText(10, 40, buf, COLOR_WHITE);

    snprintf(buf, sizeof(buf), "Reset: %s (x%u)", diag.resetReason, (unsigned)diag.resetCount);
    display.drawText(10, 70, buf, COLOR_WHITE);

    snprintf(buf, sizeof(buf), "WiFi: %d dBm (drops: %u)", diag.wifiRssi, (unsigned)diag.wifiReconnectCount);
    display.drawText(10, 100, buf, COLOR_CYAN);

    snprintf(buf, sizeof(buf), "Heap: %u KB (min %u KB)",
             (unsigned)(diag.freeHeapBytes / 1024), (unsigned)(diag.minFreeHeapBytes / 1024));
    display.drawText(10, 130, buf, COLOR_YELLOW);

    snprintf(buf, sizeof(buf), "Max alloc: %u KB", (unsigned)(diag.maxAllocBytes / 1024));
    display.drawText(10, 160, buf, COLOR_YELLOW);

    snprintf(buf, sizeof(buf), "Interventions: %u", (unsigned)diag.interventionTotal);
    display.drawText(10, 190, buf, COLOR_GREEN);

    display.drawButton(ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y,
                        ControlsScreen::TAB_RECT.w, ControlsScreen::TAB_RECT.h, "BACK", COLOR_BLUE);
}
