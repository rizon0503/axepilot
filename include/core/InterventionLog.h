#pragma once
#include <cstdint>
#include <cstddef>
#include <ctime>

// Ring buffer of the last settings changes applied to the miner: who did it,
// when and what was set. Answers the "why is my miner at 400 MHz?" question
// via the /history Telegram command.
class InterventionLog {
public:
    static constexpr size_t CAPACITY = 10;

    // epochSec = 0 means NTP wasn't synced yet when this entry was recorded;
    // format() falls back to a device-uptime-relative "5m ago" for it.
    void add(uint32_t nowMs, time_t epochSec, const char* source, int frequency, int coreVoltage);
    size_t size() const;
    uint32_t totalCount() const; // lifetime counter, never wraps with the ring

    // Newest-first, one entry per line: "14:32 UTC — Autopilot: 490 MHz / 1100 mV"
    // once NTP has synced, "5m ago — ..." (relative to nowMs) otherwise.
    void format(char* buf, size_t bufLen, uint32_t nowMs) const;

private:
    struct Entry {
        uint32_t timeMs;
        time_t epochSec;
        char source[20];
        int frequency;
        int coreVoltage;
    };

    Entry entries[CAPACITY] = {};
    size_t count = 0;
    size_t head = 0; // next write position
    uint32_t total = 0;
};
