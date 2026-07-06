#include "core/DailyStats.h"
#include <cstdio>

void DailyStats::resetAccumulators() {
    samples = 0;
    sumTemp = minTemp = maxTemp = 0.0f;
    sumHash = sumPower = 0.0f;
    haveShares = false;
    firstShares = lastShares = 0;
    firstRejected = lastRejected = 0;
}

void DailyStats::record(const BitaxeData& data, uint32_t nowMs) {
    (void)nowMs;
    if (data.temperature <= 0.0f) {
        return; // sensor glitch
    }

    if (samples == 0) {
        minTemp = maxTemp = data.temperature;
    } else {
        if (data.temperature < minTemp) minTemp = data.temperature;
        if (data.temperature > maxTemp) maxTemp = data.temperature;
    }
    sumTemp += data.temperature;
    sumHash += data.hashrate;
    sumPower += data.power;
    samples++;

    // Shares counters are cumulative on the miner: keep first and last
    if (!haveShares) {
        haveShares = true;
        firstShares = data.sharesAccepted;
        firstRejected = data.sharesRejected;
    }
    lastShares = data.sharesAccepted;
    lastRejected = data.sharesRejected;
}

std::string DailyStats::tick(uint32_t nowMs, uint32_t interventionsTotal) {
    if (!started) {
        started = true;
        periodStartMs = nowMs;
        interventionsAtStart = interventionsTotal;
        return "";
    }
    if (nowMs - periodStartMs < PERIOD_MS) {
        return "";
    }

    std::string digest;
    if (samples > 0) {
        float avgTemp = sumTemp / samples;
        float avgHash = sumHash / samples;
        float avgPower = sumPower / samples;

        char efficiency[32] = "";
        if (avgPower > 0.1f) {
            snprintf(efficiency, sizeof(efficiency), " (%.1f GH/W)", avgHash / avgPower);
        }

        char buf[512];
        snprintf(buf, sizeof(buf),
                 "📅 AxePilot daily digest:\n\n"
                 "🌡️ Temperature: avg %.1f°C (min %.1f / max %.1f)\n"
                 "⛏️ Hashrate: avg %.0f GH/s\n"
                 "⚡ Power: avg %.1f W%s\n"
                 "📈 Shares today: +%u (❌ %u)\n"
                 "🤖 Interventions today: %u",
                 avgTemp, minTemp, maxTemp,
                 avgHash,
                 avgPower, efficiency,
                 (unsigned)(lastShares - firstShares), (unsigned)(lastRejected - firstRejected),
                 (unsigned)(interventionsTotal - interventionsAtStart));
        digest = buf;
    } else {
        digest = "📅 AxePilot daily digest: no valid telemetry in the last 24h (miner offline?).";
    }

    resetAccumulators();
    periodStartMs = nowMs;
    interventionsAtStart = interventionsTotal;
    return digest;
}
