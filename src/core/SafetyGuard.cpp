#include "core/SafetyGuard.h"
#include "core/Limits.h"
#include <cstdio>

SafetyGuard::SafetyGuard(BitaxeController& miner) : miner(miner), throttled(false) {}

std::string SafetyGuard::check(const BitaxeData& data) {
    if (data.temperature <= 0.0f) {
        return ""; // Invalid sensor reading — never act on it
    }

    if (throttled) {
        if (data.temperature < Limits::TEMP_MAX) {
            throttled = false; // Cooled down — re-arm for the next incident
        }
        return "";
    }

    if (data.temperature >= Limits::TEMP_PANIC) {
        miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "SafetyGuard");
        throttled = true;

        char msg[224];
        snprintf(msg, sizeof(msg),
                 "🔥 PANIC %.1f°C >= %.1f°C!\nLocal protection (no AI) reset settings to %d MHz / %d mV.",
                 data.temperature, Limits::TEMP_PANIC, Limits::FREQ_MIN, Limits::VOLT_MIN);
        return std::string(msg);
    }

    return "";
}
