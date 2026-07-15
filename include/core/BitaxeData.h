#pragma once
#include <stdint.h>

// Snapshot of Axe OS /api/system/info telemetry.
// Deliberately a trivially-copyable POD (char[] instead of std::string):
// main.cpp copies it inside a portMUX critical section where heap
// allocation is forbidden.
struct BitaxeData {
    float temperature = 0.0f;   // °C, 0.0 = invalid sensor reading
    float hashrate = 0.0f;      // GH/s
    int coreVoltage = 0;        // mV
    int frequency = 0;          // MHz
    float power = 0.0f;         // W
    float vrTemp = 0.0f;        // °C, voltage-regulator temp; 0.0 = unsupported/invalid
    float errorPercentage = 0.0f; // ASIC hardware error rate, distinct from pool share rejections
    int fanSpeedPercent = 0;
    int fanRpm = 0;
    uint32_t sharesAccepted = 0;
    uint32_t sharesRejected = 0;
    uint32_t uptimeSeconds = 0; // miner uptime, not ESP uptime
    char bestDiff[16] = "";     // e.g. "43.9M"
    char stratumURL[64] = "";   // pool hostname, e.g. "public-pool.io"
    int stratumPort = 0;
    char stratumUser[96] = ""; // typically "wallet_address.worker_name"
    bool isOverheating = false;
    bool isTooCold = false;
};
