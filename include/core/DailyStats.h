#pragma once
#include "BitaxeData.h"
#include <string>
#include <cstdint>

// Accumulates a full day of telemetry and emits a Telegram digest once per
// 24 h. The board has no RTC/NTP, so periods are measured in uptime.
class DailyStats {
public:
    static constexpr uint32_t PERIOD_MS = 86400000UL; // 24 h

    void record(const BitaxeData& data, uint32_t nowMs);
    // Returns the digest once per period (then starts a new one), "" otherwise.
    std::string tick(uint32_t nowMs, uint32_t interventionsTotal);

private:
    bool started = false;
    uint32_t periodStartMs = 0;
    uint32_t interventionsAtStart = 0;

    int samples = 0;
    float sumTemp = 0.0f;
    float minTemp = 0.0f;
    float maxTemp = 0.0f;
    float sumHash = 0.0f;
    float sumPower = 0.0f;
    bool haveShares = false;
    uint32_t firstShares = 0;
    uint32_t lastShares = 0;
    uint32_t firstRejected = 0;
    uint32_t lastRejected = 0;

    void resetAccumulators();
};
