#pragma once
#include <stdint.h>
#include <ctime>

class ISystemTime {
public:
    virtual ~ISystemTime() = default;
    virtual uint32_t millis() = 0;
    virtual void delay(uint32_t ms) = 0;

    // Unix epoch seconds (wall-clock, UTC, NTP-synced). Returns 0 if not yet
    // synced (no WiFi at boot, NTP unreachable, etc.) — callers must treat 0
    // as "unknown" and fall back to relative/uptime-based timing.
    virtual time_t epochSeconds() = 0;
};
