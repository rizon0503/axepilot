#pragma once
#include <cstddef>

// Single source of truth for thermal thresholds and safe tuning ranges.
// Every code path that changes frequency/voltage (AI autopilot, Telegram
// commands, emergency throttle) MUST validate against these limits.
namespace Limits {
    constexpr float TEMP_MAX   = 70.0f; // Overheat: throttle down + Telegram alert
    constexpr float TEMP_PANIC = 72.0f; // Local emergency throttle, no cloud involved
    constexpr float TEMP_COLD  = 60.0f; // Underperforming: allow speed up, reset alert latch

    constexpr int FREQ_MIN = 400;  // MHz
    constexpr int FREQ_MAX = 625;  // MHz
    constexpr int VOLT_MIN = 1000; // mV
    constexpr int VOLT_MAX = 1250; // mV

    // Manual fan speed floor: anything lower risks cooking the miner
    constexpr int FAN_MIN_PERCENT = 25;

    // ArduinoJson v7's elastic JsonDocument allocates roughly proportional to
    // input size. An abnormally large Telegram/DeepSeek/AxeOS response body
    // must never reach deserializeJson() — reject it by size first.
    constexpr size_t MAX_JSON_RESPONSE_BYTES = 8192;

    inline bool isValidSetting(int freq, int volt) {
        return freq >= FREQ_MIN && freq <= FREQ_MAX &&
               volt >= VOLT_MIN && volt <= VOLT_MAX;
    }
}
