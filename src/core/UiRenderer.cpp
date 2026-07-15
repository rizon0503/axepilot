#include "core/UiRenderer.h"
#include "core/ControlsScreen.h"
#include <cstdio>
#include <cstring>

// Mirrors TFT_eSPI's TFT_* macros (RGB565) without depending on the
// hardware-only TFT_eSPI header, keeping this file natively testable.
namespace {
constexpr uint16_t COLOR_BLACK = 0x0000;
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

// Main screen row Y-coordinates. #87 compacted the pitch from 30px to 25px
// (Temp/Hashrate/VoltFreq/Mode) and the sparkline from 24px to 20px tall,
// to make room for the Err row without moving the throttle button.
constexpr int TEMP_Y = 8, HASHRATE_Y = 33, VOLTFREQ_Y = 58, MODE_Y = 83, ERR_Y = 130, POW_Y = 156;

// Sparkline row (#2): reuses the same y-slot the Mode line used to occupy
// before Volt+Freq were combined into one row to make room. Split into two
// halves with a small gap between them.
constexpr int SPARK_Y = 106, SPARK_H = 20;
constexpr int TEMP_SPARK_X = 10, HASH_SPARK_X = 170, SPARK_W = 140;

// #87: error rate above this is flagged red instead of green — mirrors what
// AxeOS itself treats as a healthy hardware error rate.
constexpr float ERR_RATE_WARN_THRESHOLD = 5.0f;

// #64: a "T"/"H" letter drawn once (in renderMainScreenChrome(), alongside
// the other static chrome) in the leading few pixels of each sparkline's
// rect, so the two graphs aren't distinguishable by color alone. The graph
// line itself is drawn/cleared starting after this reserved margin so the
// periodic data-driven redraw in renderSparklines() never paints over — or
// needs to reproduce — the label. Sized generously (Font 4 capital letters
// run up to ~18px wide on this display) so the graph's own repaint can't
// clip the letter; verify on hardware after flashing.
constexpr int SPARK_LABEL_W = 22;
constexpr int TEMP_GRAPH_X = TEMP_SPARK_X + SPARK_LABEL_W;
constexpr int HASH_GRAPH_X = HASH_SPARK_X + SPARK_LABEL_W;
constexpr int GRAPH_W = SPARK_W - SPARK_LABEL_W;
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

void UiRenderer::drawSparklineLine(int x, int y, int w, int h, const float* values, size_t count, uint16_t color) {
    if (count < 2) {
        return; // nothing to connect
    }
    float minV = values[0];
    float maxV = values[0];
    for (size_t i = 1; i < count; i++) {
        if (values[i] < minV) minV = values[i];
        if (values[i] > maxV) maxV = values[i];
    }
    float range = maxV - minV;

    auto yFor = [&](float v) -> int {
        if (range < 0.001f) {
            return y + h / 2; // flat data: draw a level line instead of dividing by ~0
        }
        float normalized = (v - minV) / range; // 0 (min) .. 1 (max)
        return y + (h - 1) - (int)(normalized * (h - 1));
    };

    for (size_t i = 0; i + 1 < count; i++) {
        int x0 = x + (int)((long)i * (w - 1) / (count - 1));
        int x1 = x + (int)((long)(i + 1) * (w - 1) / (count - 1));
        display.drawLine(x0, yFor(values[i]), x1, yFor(values[i + 1]), color);
    }
}

void UiRenderer::renderSparklines(const float* tempHistory, size_t tempHistoryCount,
                                   const float* hashHistory, size_t hashHistoryCount) {
    // Clamp defensively: lastTempSpark_/lastHashSpark_ are fixed
    // MAX_SPARKLINE_SAMPLES-sized buffers, so an over-large count here
    // would overflow them via memcpy below. Currently unreachable (the
    // only caller passes at most TelemetryHistory::CAPACITY == 20), but a
    // buffer-safety bound isn't something to skip just because it's not
    // hit today.
    if (tempHistoryCount > MAX_SPARKLINE_SAMPLES) tempHistoryCount = MAX_SPARKLINE_SAMPLES;
    if (hashHistoryCount > MAX_SPARKLINE_SAMPLES) hashHistoryCount = MAX_SPARKLINE_SAMPLES;

    bool tempChanged = tempHistoryCount != lastTempSparkCount_ ||
                        memcmp(tempHistory, lastTempSpark_, tempHistoryCount * sizeof(float)) != 0;
    if (tempChanged) {
        display.fillRect(TEMP_GRAPH_X, SPARK_Y, GRAPH_W, SPARK_H, COLOR_BLACK);
        drawSparklineLine(TEMP_GRAPH_X, SPARK_Y, GRAPH_W, SPARK_H, tempHistory, tempHistoryCount, COLOR_GREEN);
        memcpy(lastTempSpark_, tempHistory, tempHistoryCount * sizeof(float));
        lastTempSparkCount_ = tempHistoryCount;
    }

    bool hashChanged = hashHistoryCount != lastHashSparkCount_ ||
                        memcmp(hashHistory, lastHashSpark_, hashHistoryCount * sizeof(float)) != 0;
    if (hashChanged) {
        display.fillRect(HASH_GRAPH_X, SPARK_Y, GRAPH_W, SPARK_H, COLOR_BLACK);
        drawSparklineLine(HASH_GRAPH_X, SPARK_Y, GRAPH_W, SPARK_H, hashHistory, hashHistoryCount, COLOR_WHITE);
        memcpy(lastHashSpark_, hashHistory, hashHistoryCount * sizeof(float));
        lastHashSparkCount_ = hashHistoryCount;
    }
}

void UiRenderer::renderTelemetry(const BitaxeData& data, OperationMode mode, bool wifiOk,
                                  const float* tempHistory, size_t tempHistoryCount,
                                  const float* hashHistory, size_t hashHistoryCount) {
    char buf[64];

    snprintf(buf, sizeof(buf), "Temp: %.1f C", data.temperature);
    drawIfChanged(10, TEMP_Y, buf, data.isOverheating ? COLOR_RED : COLOR_GREEN, tempCache_);

    // If hashrate is crazy high (like 86000 GH/s), display it in TH/s to save screen space
    if (data.hashrate > 9999.0f) {
        snprintf(buf, sizeof(buf), "Hashrate: %.1f TH/s", data.hashrate / 1000.0f);
    } else {
        snprintf(buf, sizeof(buf), "Hashrate: %.1f GH/s", data.hashrate);
    }
    drawIfChanged(10, HASHRATE_Y, buf, COLOR_WHITE, hashrateCache_);

    // Volt+Freq combined into one row (#2) to make room for the sparkline
    // row below, which used to be the Mode row's slot.
    snprintf(buf, sizeof(buf), "V:%dmV F:%dMHz", data.coreVoltage, data.frequency);
    drawIfChanged(10, VOLTFREQ_Y, buf, COLOR_YELLOW, voltFreqCache_);

    if (!wifiOk) {
        drawIfChanged(10, MODE_Y, "Wi-Fi reconnecting...", COLOR_RED, modeCache_);
    } else {
        snprintf(buf, sizeof(buf), "Mode: %s", mode == OperationMode::AUTOPILOT ? "AUTO" : "MANUAL");
        drawIfChanged(10, MODE_Y, buf, mode == OperationMode::AUTOPILOT ? COLOR_GREEN : COLOR_ORANGE, modeCache_);
    }

    renderSparklines(tempHistory, tempHistoryCount, hashHistory, hashHistoryCount);

    // #87: ASIC hardware error rate, distinct from pool share rejections —
    // matters most right after a freq/volt change, since unstable settings
    // show up here first.
    snprintf(buf, sizeof(buf), "Err: %.1f%%", data.errorPercentage);
    drawIfChanged(10, ERR_Y, buf, data.errorPercentage > ERR_RATE_WARN_THRESHOLD ? COLOR_RED : COLOR_GREEN, errCache_);

    // In autofanspeed mode AxeOS reports fanspeed=0%, so RPM is the
    // meaningful number; show the percent only when it is set manually.
    if (data.fanSpeedPercent > 0) {
        snprintf(buf, sizeof(buf), "Pow: %.1fW Fan: %d%%", data.power, data.fanSpeedPercent);
    } else {
        snprintf(buf, sizeof(buf), "Pow: %.1fW Fan: %drpm", data.power, data.fanRpm);
    }
    drawIfChanged(10, POW_Y, buf, COLOR_WHITE, powCache_);
}

void UiRenderer::resetTelemetryCache() {
    tempCache_.text[0] = '\0';
    hashrateCache_.text[0] = '\0';
    voltFreqCache_.text[0] = '\0';
    modeCache_.text[0] = '\0';
    errCache_.text[0] = '\0';
    powCache_.text[0] = '\0';
    lastTempSparkCount_ = 0;
    lastHashSparkCount_ = 0;
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

    // Sparkline labels (#64): static, so drawn once here rather than on
    // every renderTelemetry() call like the graphs themselves.
    display.drawText(TEMP_SPARK_X, SPARK_Y, "T", COLOR_GREEN);
    display.drawText(HASH_SPARK_X, SPARK_Y, "H", COLOR_WHITE);
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

// OTA progress bar geometry: 300px = 100%, so bar width is percent * 3.
namespace {
constexpr int OTA_BAR_X = 10, OTA_BAR_Y = 150, OTA_BAR_H = 20, OTA_BAR_FULL_W = 300;
} // namespace

void UiRenderer::renderOtaScreen() {
    display.clear();
    display.drawText(10, 40, "OTA update...", COLOR_CYAN);
    display.drawText(10, 70, "Do not power off", COLOR_WHITE);
}

void UiRenderer::renderOtaProgress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    display.drawText(10, 110, buf, COLOR_WHITE);
    display.fillRect(OTA_BAR_X, OTA_BAR_Y, percent * OTA_BAR_FULL_W / 100, OTA_BAR_H, COLOR_GREEN);
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
