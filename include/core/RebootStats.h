#pragma once
#include <atomic>
#include <cstdint>
#include "interfaces/ISettingsStore.h"

// Persists a handful of lifetime counters across reboots/power loss via
// ISettingsStore: total reset count, the longest uptime ever recorded, and
// the cumulative autopilot intervention total (InterventionLog::totalCount()
// otherwise resets to zero on every boot). Writes are throttled to limit
// NVS flash wear.
//
// begin()/tick() are called from the network task (core 0); the getters
// are also read from the UI loop (core 1) by the Diagnostics screen (#3) —
// hence std::atomic instead of plain uint32_t fields.
class RebootStats {
public:
    static constexpr uint32_t UPTIME_SAVE_THRESHOLD_SECONDS = 60;

    explicit RebootStats(ISettingsStore& store);

    // Increments and immediately persists the reset counter (this boot),
    // and loads the previously saved uptime record / intervention total.
    // Must be called exactly once, from a context where touching NVS is
    // safe (e.g. setup()) — not from a global object's constructor.
    void begin();

    uint32_t resetCount() const { return resetCountValue; }
    uint32_t uptimeRecordSeconds() const { return uptimeRecordValue; }
    uint32_t interventionTotal() const { return lastSavedInterventionTotal; }

    // Call periodically (e.g. once per telemetry tick). Persists the
    // uptime record only once this boot's uptime grows at least
    // UPTIME_SAVE_THRESHOLD_SECONDS past the last saved value (never
    // regresses it), and the intervention total only when it changes.
    void tick(uint32_t uptimeSeconds, uint32_t currentInterventionTotal);

private:
    ISettingsStore& store;
    std::atomic<uint32_t> resetCountValue{0};
    std::atomic<uint32_t> uptimeRecordValue{0};
    std::atomic<uint32_t> lastSavedInterventionTotal{0};
};
