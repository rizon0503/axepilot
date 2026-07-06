#include "core/TelemetryHistory.h"
#include <cstdio>

void TelemetryHistory::record(const BitaxeData& data, uint32_t nowMs) {
    if (data.temperature <= 0.0f) {
        return; // sensor glitch — do not poison the history
    }
    if (count > 0 && nowMs - lastRecordMs < INTERVAL_MS) {
        return;
    }
    lastRecordMs = nowMs;

    samples[head] = {nowMs, data.temperature, data.hashrate};
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY) {
        count++;
    }
}

size_t TelemetryHistory::size() const {
    return count;
}

const TelemetryHistory::Sample& TelemetryHistory::oldest() const {
    return samples[(head + CAPACITY - count) % CAPACITY];
}

const TelemetryHistory::Sample& TelemetryHistory::newest() const {
    return samples[(head + CAPACITY - 1) % CAPACITY];
}

float TelemetryHistory::tempTrendPerMinute() const {
    if (count < 2) {
        return 0.0f;
    }
    uint32_t spanMs = newest().timeMs - oldest().timeMs;
    if (spanMs < 1000) {
        return 0.0f;
    }
    return (newest().temperature - oldest().temperature) / (spanMs / 60000.0f);
}

float TelemetryHistory::avgTemperature() const {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += samples[(head + CAPACITY - count + i) % CAPACITY].temperature;
    }
    return sum / count;
}

float TelemetryHistory::avgHashrate() const {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += samples[(head + CAPACITY - count + i) % CAPACITY].hashrate;
    }
    return sum / count;
}

void TelemetryHistory::summarize(char* buf, size_t bufLen) const {
    if (count < 2) {
        if (bufLen > 0) buf[0] = '\0';
        return;
    }
    float spanMin = (newest().timeMs - oldest().timeMs) / 60000.0f;
    snprintf(buf, bufLen,
             " History: temp trend %+.2fC/min over %.1f min, avg temp %.1fC, avg hashrate %.0f GH/s (%u samples).",
             tempTrendPerMinute(), spanMin, avgTemperature(), avgHashrate(), (unsigned)count);
}
