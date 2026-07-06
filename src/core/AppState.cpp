#include "core/AppState.h"

namespace AppState {

ScreenPower::ScreenPower(uint32_t idleTimeoutMs, uint32_t wakeDebounceMs)
    : idleTimeoutMs_(idleTimeoutMs), wakeDebounceMs_(wakeDebounceMs) {}

bool ScreenPower::onTouch(bool touched, uint32_t now) {
    justWoke_ = false;
    if (!touched) {
        return false;
    }

    lastInteractionAt_ = now;

    if (!isOn_) {
        isOn_ = true;
        justWoke_ = true;
        ignoreTouchUntil_ = now + wakeDebounceMs_;
        return false; // this touch only wakes the screen
    }

    if ((int32_t)(now - ignoreTouchUntil_) < 0) {
        return false; // still inside the post-wake debounce window
    }

    return true;
}

bool ScreenPower::tick(uint32_t now) {
    if (isOn_ && (now - lastInteractionAt_ > idleTimeoutMs_)) {
        isOn_ = false;
        return true;
    }
    return false;
}

bool ScreenPower::isOn() const {
    return isOn_;
}

bool ScreenPower::justWoke() const {
    return justWoke_;
}

void RestoreTimer::trigger(uint32_t now, uint32_t durationMs) {
    restoreAt_ = now + durationMs;
}

bool RestoreTimer::isPending() const {
    return restoreAt_ != 0;
}

bool RestoreTimer::tick(uint32_t now) {
    if (restoreAt_ != 0 && (int32_t)(now - restoreAt_) >= 0) {
        restoreAt_ = 0;
        return true;
    }
    return false;
}

} // namespace AppState
