#pragma once
#include "BitaxeData.h"
#include <cstdint>
#include <cstddef>

// Fixed-size ring buffer of recent telemetry samples (one per 30 s, 20 slots
// = 10 minutes of history). Gives the AI autopilot and /status the direction
// and rate of temperature change instead of a single snapshot.
class TelemetryHistory {
public:
    static constexpr size_t CAPACITY = 20;
    static constexpr uint32_t INTERVAL_MS = 30000;

    // Rate-limited: stores at most one sample per INTERVAL_MS.
    // Samples with an invalid (<= 0) temperature are ignored.
    void record(const BitaxeData& data, uint32_t nowMs);

    size_t size() const;
    float tempTrendPerMinute() const; // °C per minute over the whole window
    float avgTemperature() const;
    float avgHashrate() const;
    float avgPower() const;

    // Compact English one-liner for AI prompts; empty string when < 2 samples.
    void summarize(char* buf, size_t bufLen) const;

    // Copies up to min(maxCount, size()) samples, oldest-first, into the
    // caller's buffers — for the main-screen sparkline (#2), which needs
    // the individual values rather than an aggregate. Returns how many
    // were copied.
    size_t copySparklineData(float* outTemps, float* outHashrates, size_t maxCount) const;

private:
    struct Sample {
        uint32_t timeMs;
        float temperature;
        float hashrate;
        float power;
    };

    Sample samples[CAPACITY] = {};
    size_t count = 0;
    size_t head = 0; // next write position
    uint32_t lastRecordMs = 0;

    const Sample& oldest() const;
    const Sample& newest() const;
};
