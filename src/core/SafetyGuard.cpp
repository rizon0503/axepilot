#include "core/SafetyGuard.h"
#include "core/Limits.h"
#include <cstdio>

SafetyGuard::SafetyGuard(BitaxeController& miner) : miner(miner), throttled(false) {}

std::string SafetyGuard::check(const BitaxeData& data) {
    bool chipReading = data.temperature > 0.0f;
    bool vrReading = data.vrTemp > 0.0f;
    if (chipReading) chipEverValid = true;
    if (vrReading) vrEverValid = true;

    if (throttled) {
        // A sensor that has never once reported a valid value (this
        // hardware doesn't have it) never blocks re-arming. A sensor that
        // DOES normally report but glitches to an invalid reading this one
        // cycle must NOT be treated as cooled — freeze the latch instead of
        // guessing, same conservatism the chip-only check always had.
        bool chipCooled = chipReading ? (data.temperature < Limits::TEMP_MAX) : !chipEverValid;
        bool vrCooled = vrReading ? (data.vrTemp < Limits::VR_TEMP_PANIC) : !vrEverValid;
        if (chipCooled && vrCooled) {
            throttled = false;
        }
        return "";
    }

    if (chipReading && data.temperature >= Limits::TEMP_PANIC) {
        miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "SafetyGuard");
        throttled = true;

        char msg[224];
        snprintf(msg, sizeof(msg),
                 "🔥 PANIC %.1f°C >= %.1f°C!\nLocal protection (no AI) reset settings to %d MHz / %d mV.",
                 data.temperature, Limits::TEMP_PANIC, Limits::FREQ_MIN, Limits::VOLT_MIN);
        return std::string(msg);
    }

    if (vrReading && data.vrTemp >= Limits::VR_TEMP_PANIC) {
        miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "SafetyGuard");
        throttled = true;

        char msg[224];
        snprintf(msg, sizeof(msg),
                 "🔥 VR PANIC %.1f°C >= %.1f°C!\nLocal protection (no AI) reset settings to %d MHz / %d mV.",
                 data.vrTemp, Limits::VR_TEMP_PANIC, Limits::FREQ_MIN, Limits::VOLT_MIN);
        return std::string(msg);
    }

    return "";
}
