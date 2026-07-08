#include "core/DailyStats.h"
#include <cstdio>
#include <ctime>

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

void DailyStats::initializeLastFiredDay(time_t nowEpoch) {
    time_t today = nowEpoch / 86400;
    const struct tm* tmInfo = gmtime(&nowEpoch);
    // Past today's digest hour: skip today (no full day of data yet), wait
    // for tomorrow. Before it: allow today's digest once the clock reaches
    // DIGEST_HOUR_UTC.
    lastFiredDay = (tmInfo->tm_hour >= DIGEST_HOUR_UTC) ? today : today - 1;
}

std::string DailyStats::tick(uint32_t nowMs, time_t nowEpoch, uint32_t interventionsTotal) {
    if (!started) {
        started = true;
        periodStartMs = nowMs;
        interventionsAtStart = interventionsTotal;
        if (nowEpoch > 0) {
            initializeLastFiredDay(nowEpoch);
        }
        return "";
    }

    bool shouldFire;
    if (nowEpoch > 0) {
        if (lastFiredDay == -1) {
            // NTP just became available for the first time this boot
            // (configTime() is asynchronous, so nowEpoch was still 0 on
            // the first tick() above) — establish a baseline instead of
            // letting the "-1 = never fired" sentinel fire immediately
            // regardless of time of day.
            initializeLastFiredDay(nowEpoch);
        }
        time_t today = nowEpoch / 86400;
        const struct tm* tmInfo = gmtime(&nowEpoch);
        shouldFire = (today != lastFiredDay && tmInfo->tm_hour >= DIGEST_HOUR_UTC);
        if (shouldFire) {
            lastFiredDay = today;
        }
    } else {
        // No NTP sync (yet, or ever) — fall back to the original cadence
        shouldFire = (nowMs - periodStartMs >= PERIOD_MS);
    }
    if (!shouldFire) {
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
