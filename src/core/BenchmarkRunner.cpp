#include "core/BenchmarkRunner.h"
#include "core/Limits.h"
#include <cstdio>

const BenchmarkRunner::Preset BenchmarkRunner::PRESETS[BenchmarkRunner::PRESET_COUNT] = {
    {400, 1000}, {490, 1060}, {525, 1100}, {550, 1150}, {600, 1200}, {625, 1250},
};

BenchmarkRunner::BenchmarkRunner(BitaxeController& miner) : miner(miner) {}

bool BenchmarkRunner::isRunning() const {
    return running;
}

void BenchmarkRunner::applyStep(uint32_t nowMs) {
    miner.applySettings(PRESETS[step].freq, PRESETS[step].volt, "Benchmark");
    stepStartMs = nowMs;
    lastSampleMs = 0;
}

std::string BenchmarkRunner::start(uint32_t nowMs) {
    if (running) {
        return "🔬 Benchmark is already running. /bench status — progress, /bench stop — stop it.";
    }
    running = true;
    step = 0;
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        results[i] = Result{};
    }
    applyStep(nowMs);

    char msg[224];
    snprintf(msg, sizeof(msg),
             "🔬 Benchmark started: %u steps of %u min (%u min warmup per step), ~%u min total.\nStep 1/%u: %d MHz / %d mV",
             (unsigned)PRESET_COUNT, (unsigned)(DWELL_MS / 60000), (unsigned)(WARMUP_MS / 60000),
             (unsigned)(PRESET_COUNT * DWELL_MS / 60000), (unsigned)PRESET_COUNT,
             PRESETS[0].freq, PRESETS[0].volt);
    return std::string(msg);
}

std::string BenchmarkRunner::stop() {
    if (!running) {
        return "Benchmark is not running.";
    }
    running = false;
    miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "Benchmark stop");
    return "🛑 Benchmark stopped manually. Safe settings applied.\n" + partialReport();
}

std::string BenchmarkRunner::tick(const BitaxeData& data, uint32_t nowMs) {
    if (!running) {
        return "";
    }
    if (data.temperature <= 0.0f) {
        return ""; // invalid sensor reading — don't measure, don't abort
    }

    if (data.temperature >= Limits::TEMP_MAX) {
        running = false;
        miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "Benchmark abort");
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "🛑 Benchmark aborted: %.1f°C at step %u (%d MHz / %d mV). Safe settings applied.\n",
                 data.temperature, (unsigned)(step + 1), PRESETS[step].freq, PRESETS[step].volt);
        return std::string(msg) + partialReport();
    }

    uint32_t inStep = nowMs - stepStartMs;

    if (inStep >= WARMUP_MS && (lastSampleMs == 0 || nowMs - lastSampleMs >= SAMPLE_INTERVAL_MS)) {
        lastSampleMs = nowMs;
        results[step].sumHash += data.hashrate;
        results[step].sumTemp += data.temperature;
        results[step].sumPower += data.power;
        results[step].samples++;
    }

    if (inStep >= DWELL_MS) {
        results[step].completed = results[step].samples > 0;
        step++;
        if (step >= PRESET_COUNT) {
            running = false;
            return finishReport();
        }
        applyStep(nowMs);
        char msg[96];
        snprintf(msg, sizeof(msg), "🔬 Step %u/%u: %d MHz / %d mV",
                 (unsigned)(step + 1), (unsigned)PRESET_COUNT, PRESETS[step].freq, PRESETS[step].volt);
        return std::string(msg);
    }

    return "";
}

std::string BenchmarkRunner::progress(uint32_t nowMs) const {
    if (!running) {
        return "Benchmark is not running. /bench start — begin it (~30 min).";
    }
    uint32_t inStep = nowMs - stepStartMs;
    uint32_t leftSec = (inStep < DWELL_MS) ? (DWELL_MS - inStep) / 1000 : 0;
    char msg[192];
    snprintf(msg, sizeof(msg),
             "🔬 Step %u/%u: %d MHz / %d mV\nSamples: %d, next step in: %u min %u s",
             (unsigned)(step + 1), (unsigned)PRESET_COUNT, PRESETS[step].freq, PRESETS[step].volt,
             results[step].samples, (unsigned)(leftSec / 60), (unsigned)(leftSec % 60));
    return std::string(msg);
}

std::string BenchmarkRunner::partialReport() const {
    std::string report;
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        if (!results[i].completed || results[i].samples == 0) continue;
        float avgHash = results[i].sumHash / results[i].samples;
        float avgTemp = results[i].sumTemp / results[i].samples;
        float avgPower = results[i].sumPower / results[i].samples;
        char line[112];
        if (avgPower > 0.1f) {
            snprintf(line, sizeof(line), "%d MHz / %d mV: %.0f GH/s, %.1f°C, %.1f W (%.1f GH/W)\n",
                     PRESETS[i].freq, PRESETS[i].volt, avgHash, avgTemp, avgPower, avgHash / avgPower);
        } else {
            snprintf(line, sizeof(line), "%d MHz / %d mV: %.0f GH/s, %.1f°C, %.1f W\n",
                     PRESETS[i].freq, PRESETS[i].volt, avgHash, avgTemp, avgPower);
        }
        report += line;
    }
    return report.empty() ? "No completed steps yet." : report;
}

std::string BenchmarkRunner::finishReport() {
    int bestIdx = -1;
    float bestEff = 0.0f;
    for (size_t i = 0; i < PRESET_COUNT; i++) {
        if (!results[i].completed || results[i].samples == 0) continue;
        float avgTemp = results[i].sumTemp / results[i].samples;
        float avgPower = results[i].sumPower / results[i].samples;
        if (avgTemp >= Limits::TEMP_MAX || avgPower <= 0.1f) continue;
        float eff = (results[i].sumHash / results[i].samples) / avgPower;
        if (eff > bestEff) {
            bestEff = eff;
            bestIdx = (int)i;
        }
    }

    std::string report = "🏁 Benchmark finished!\n\n" + partialReport();

    if (bestIdx >= 0) {
        miner.applySettings(PRESETS[bestIdx].freq, PRESETS[bestIdx].volt, "Benchmark optimum");
        char best[128];
        snprintf(best, sizeof(best), "\n🏆 Optimum: %d MHz / %d mV (%.1f GH/W) — applied.",
                 PRESETS[bestIdx].freq, PRESETS[bestIdx].volt, bestEff);
        report += best;
    } else {
        miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "Benchmark fallback");
        report += "\n⚠️ No step stayed within the thermal envelope — safe settings applied.";
    }
    return report;
}
