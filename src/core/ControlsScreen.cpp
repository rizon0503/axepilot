#include "core/ControlsScreen.h"
#include "core/TouchMapper.h"
#include "core/Limits.h"

namespace ControlsScreen {

namespace {
bool within(int x, int y, const Rect& r) {
    return TouchMapper::isWithinRect(x, y, r.x, r.y, r.w, r.h);
}
} // namespace

Action hitTest(int screenX, int screenY) {
    if (within(screenX, screenY, PRESET_ECO_RECT)) return Action::PRESET_ECO;
    if (within(screenX, screenY, PRESET_BALANCED_RECT)) return Action::PRESET_BALANCED;
    if (within(screenX, screenY, PRESET_TURBO_RECT)) return Action::PRESET_TURBO;
    if (within(screenX, screenY, PRESET_MAX_RECT)) return Action::PRESET_MAX;
    if (within(screenX, screenY, TOGGLE_MODE_RECT)) return Action::TOGGLE_MODE;
    if (within(screenX, screenY, RESTART_RECT)) return Action::RESTART;
    if (within(screenX, screenY, TAB_RECT)) return Action::SWITCH_SCREEN;
    return Action::NONE;
}

Action hitTestMainScreen(int screenX, int screenY) {
    if (within(screenX, screenY, TAB_RECT)) return Action::SWITCH_SCREEN;
    return Action::NONE;
}

Preset presetFor(Action action) {
    switch (action) {
        case Action::PRESET_ECO:      return {Limits::FREQ_MIN, Limits::VOLT_MIN};
        case Action::PRESET_BALANCED: return {500, 1150};
        case Action::PRESET_TURBO:    return {575, 1200};
        case Action::PRESET_MAX:      return {Limits::FREQ_MAX, Limits::VOLT_MAX};
        default:                      return {0, 0};
    }
}

} // namespace ControlsScreen
