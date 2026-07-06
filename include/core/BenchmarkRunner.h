#pragma once
#include "BitaxeController.h"
#include "BitaxeData.h"
#include <string>
#include <cstdint>
#include <cstddef>

// Steps through safe freq/volt presets, dwells on each one, averages the
// telemetry and reports the most efficient (GH/W) stable point. Runs inside
// the network task via tick(); aborts itself the moment the miner overheats.
class BenchmarkRunner {
public:
    static constexpr uint32_t WARMUP_MS = 120000;         // settle time, not measured
    static constexpr uint32_t DWELL_MS = 300000;          // total time per preset
    static constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;  // measurement cadence

    explicit BenchmarkRunner(BitaxeController& miner);

    bool isRunning() const;
    std::string start(uint32_t nowMs);
    std::string stop();  // manual abort — restores safe settings
    // Call on every fresh telemetry sample; returns a user-facing message on
    // step change, completion or abort, empty string otherwise.
    std::string tick(const BitaxeData& data, uint32_t nowMs);
    std::string progress(uint32_t nowMs) const;

private:
    struct Preset {
        int freq;
        int volt;
    };
    struct Result {
        float sumHash = 0.0f;
        float sumTemp = 0.0f;
        float sumPower = 0.0f;
        int samples = 0;
        bool completed = false;
    };

    static constexpr size_t PRESET_COUNT = 6;
    static const Preset PRESETS[PRESET_COUNT];

    BitaxeController& miner;
    bool running = false;
    size_t step = 0;
    uint32_t stepStartMs = 0;
    uint32_t lastSampleMs = 0;
    Result results[PRESET_COUNT];

    void applyStep(uint32_t nowMs);
    std::string partialReport() const;
    std::string finishReport();
};
