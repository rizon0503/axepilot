#pragma once
#include <cstdint>

// Screen timeout, wake debounce, and button-restore timers, extracted from
// raw millis() comparisons scattered across main.cpp's loop() (#15).
// These are pure and hardware-independent so they're testable on the host.
namespace AppState {

// Screen backlight on/off timeout, with a debounce window after waking so
// the same physical touch that wakes the screen doesn't also register as
// a button press one tick later.
class ScreenPower {
public:
    explicit ScreenPower(uint32_t idleTimeoutMs = 15000, uint32_t wakeDebounceMs = 300);

    // Feed this tick's raw touch reading + current time. Returns whether
    // the touch should be treated as active for UI dispatch this tick —
    // false while the screen is waking up, or for the rest of the
    // debounce window right after waking.
    bool onTouch(bool touched, uint32_t now);

    // Call every tick regardless of touch. Returns true exactly once, the
    // tick the screen should turn off due to inactivity.
    bool tick(uint32_t now);

    bool isOn() const;
    // True only on the tick onTouch() just turned the screen back on.
    bool justWoke() const;

private:
    uint32_t idleTimeoutMs_;
    uint32_t wakeDebounceMs_;
    bool isOn_ = true;
    bool justWoke_ = false;
    uint32_t lastInteractionAt_ = 0;
    uint32_t ignoreTouchUntil_ = 0;
};

// Generic "restore to normal after N ms" latch, used for the emergency
// throttle and restart buttons' temporary "fired" label.
class RestoreTimer {
public:
    void trigger(uint32_t now, uint32_t durationMs);
    bool isPending() const;

    // Call every tick. Returns true exactly once, the tick the caller
    // should restore its normal appearance.
    bool tick(uint32_t now);

private:
    uint32_t restoreAt_ = 0; // 0 = not pending
};

} // namespace AppState
