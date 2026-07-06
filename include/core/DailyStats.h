#pragma once
#include "BitaxeData.h"
#include <string>
#include <cstdint>
#include <ctime>

// Accumulates a day of telemetry and emits a Telegram digest. Once NTP has
// synced (nowEpoch > 0), the digest fires once per calendar day at
// DIGEST_HOUR_UTC. Without NTP, falls back to firing every PERIOD_MS of
// device uptime (the original behavior, for a board with no RTC/NTP).
class DailyStats {
public:
    static constexpr uint32_t PERIOD_MS = 86400000UL; // 24 h, NTP-unavailable fallback
    static constexpr int DIGEST_HOUR_UTC = 9;

    void record(const BitaxeData& data, uint32_t nowMs);
    // Returns the digest once per period (then starts a new one), "" otherwise.
    std::string tick(uint32_t nowMs, time_t nowEpoch, uint32_t interventionsTotal);

private:
    bool started = false;
    uint32_t periodStartMs = 0;
    time_t lastFiredDay = -1; // days-since-epoch of the last NTP-mode digest; -1 = never
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
