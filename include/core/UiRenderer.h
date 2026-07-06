#pragma once
#include "interfaces/IDisplay.h"
#include "core/BitaxeData.h"
#include "core/OperationMode.h"
#include <cstdint>

// Screen layout, colors, and string formatting for the CYD's three screens
// (Main, Controls, Diagnostics), extracted from main.cpp so they're
// testable via an IDisplay mock instead of only verifiable by eye on real
// hardware (#14). Pure rendering only — touch dispatch and hit-testing
// stay in main.cpp/TouchMapper/ControlsScreen.
class UiRenderer {
public:
    explicit UiRenderer(IDisplay& display);

    // Main screen's live telemetry (temp/hashrate/volt/freq/power/mode),
    // meant to be called on a ~2x/second cadence by the caller. Only
    // repaints a line whose text or color actually changed since the last
    // call, to avoid the full-screen flicker an unconditional redraw would
    // cause (#41).
    void renderTelemetry(const BitaxeData& data, OperationMode mode, bool wifiOk);

    // Forces the next renderTelemetry() call to repaint every line
    // regardless of whether it changed — needed after something else
    // (switching away to another screen and back) clears the physical
    // display out from under the per-line cache above.
    void resetTelemetryCache();

    enum class ThrottleState { NORMAL, TRIGGERED };
    // Draws the Main screen's emergency-throttle button in the given state.
    void renderThrottleButton(ThrottleState state);

    // Static chrome for the Main screen: the emergency throttle button plus
    // the tab button that switches to the Controls screen. Clears the
    // screen first so a previous screen's buttons don't linger.
    void renderMainScreenChrome();

    // The Controls screen (frequency/voltage presets, AUTO/MANUAL toggle,
    // restart, tab). Small and static enough to redraw wholesale instead of
    // tracking per-line diffs like the Main screen does. Clears first.
    void renderControlsScreen(OperationMode mode);

    enum class RestartButtonState { NORMAL, SENT };
    // Draws the Controls screen's restart button in the given state.
    void renderRestartButton(RestartButtonState state);

    // ESP32 controller diagnostics shown on the Diagnostics screen — mirrors
    // the /esp Telegram command's fields. Caller gathers the values (they
    // come from ISystemInfo/RebootStats/ISystemTime, which UiRenderer has no
    // reason to depend on).
    struct DiagnosticsData {
        uint32_t uptimeSeconds = 0;
        const char* resetReason = "";
        uint32_t resetCount = 0;
        int wifiRssi = 0;
        uint32_t wifiReconnectCount = 0;
        uint32_t freeHeapBytes = 0;
        uint32_t minFreeHeapBytes = 0;
        uint32_t maxAllocBytes = 0;
        uint32_t interventionTotal = 0;
    };
    // Drawn once on entry, like the Controls screen: this is a glance-at-it
    // snapshot, not a live dashboard. Clears first.
    void renderDiagnosticsScreen(const DiagnosticsData& diag);

private:
    IDisplay& display;

    // Per-line "last drawn" cache so renderTelemetry() skips repainting a
    // line whose text/color hasn't changed since the last call.
    struct LineCache {
        char text[64] = "";
        uint16_t color = 0xFFFF;
    };
    LineCache tempCache_, hashrateCache_, voltCache_, freqCache_, powCache_, modeCache_;

    void drawIfChanged(int x, int y, const char* text, uint16_t color, LineCache& cache);
};
