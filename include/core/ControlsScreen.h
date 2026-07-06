#pragma once

// Pure, hardware-independent layout and hit-testing for the CYD "Controls"
// screen (frequency/voltage presets, AUTO/MANUAL toggle, miner restart),
// mirroring TouchMapper's role for the main screen's emergency throttle
// button so it's testable on the host.
namespace ControlsScreen {

struct Rect { int x, y, w, h; };

// Button geometry, shared by EspDisplay/main.cpp (drawing) and hitTest()
// (touch matching) so both always agree on where a button actually is.
constexpr Rect PRESET_ECO_RECT{10, 40, 140, 50};
constexpr Rect PRESET_BALANCED_RECT{170, 40, 140, 50};
constexpr Rect PRESET_TURBO_RECT{10, 100, 140, 50};
constexpr Rect PRESET_MAX_RECT{170, 100, 140, 50};
constexpr Rect TOGGLE_MODE_RECT{10, 160, 140, 50};
constexpr Rect RESTART_RECT{170, 160, 140, 50};
// Same rect on both the Main and Controls screens — switches between them.
constexpr Rect TAB_RECT{260, 0, 60, 30};

enum class Action {
    NONE,
    PRESET_ECO,
    PRESET_BALANCED,
    PRESET_TURBO,
    PRESET_MAX,
    TOGGLE_MODE,
    RESTART,
    SWITCH_SCREEN,
};

struct Preset {
    int frequency;
    int coreVoltage;
};

// Which action (if any) a calibrated screen-pixel touch on the Controls
// screen corresponds to.
Action hitTest(int screenX, int screenY);

// Which action a touch on the Main screen corresponds to, besides the
// existing emergency-throttle rect — currently just the tab button.
Action hitTestMainScreen(int screenX, int screenY);

// {0, 0} for actions that aren't a preset.
Preset presetFor(Action action);

} // namespace ControlsScreen
