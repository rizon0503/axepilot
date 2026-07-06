#pragma once
#include "BitaxeController.h"
#include "BitaxeData.h"
#include <string>

// Cloud-independent hardware protection. The AI autopilot needs a working
// internet connection and a live DeepSeek API; this guard needs neither.
// If the miner crosses TEMP_PANIC it is immediately forced to the safest
// settings, latched until it cools below TEMP_MAX to avoid PATCH spam.
class SafetyGuard {
public:
    explicit SafetyGuard(BitaxeController& miner);

    // Call on every fresh telemetry sample. Returns a non-empty alert
    // message when the guard intervenes, empty string otherwise.
    std::string check(const BitaxeData& data);

private:
    BitaxeController& miner;
    bool throttled;
    // Tracks whether each sensor has EVER reported a valid (>0) value, so a
    // momentary glitch on a normally-working sensor (freeze the latch, same
    // as before this class knew about vrTemp) can be told apart from a
    // sensor this hardware simply doesn't have (never blocks re-arming).
    bool chipEverValid = false;
    bool vrEverValid = false;
};
