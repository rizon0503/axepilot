#pragma once
#include <cstdint>

// Runtime diagnostics of the microcontroller itself (not the miner).
class ISystemInfo {
public:
    virtual ~ISystemInfo() = default;
    virtual int wifiRssi() = 0;
    virtual uint32_t freeHeapBytes() = 0;
    virtual uint32_t maxAllocBytes() = 0;
    virtual uint32_t minFreeHeapBytes() = 0;   // lowest free heap since boot
    virtual const char* resetReason() = 0;     // human-readable last reset cause

    // WiFi drop accounting: the network task reports each lost connection
    virtual void noteWifiReconnect() = 0;
    virtual uint32_t wifiReconnectCount() = 0;
};
