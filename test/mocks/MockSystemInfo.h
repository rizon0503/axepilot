#pragma once
#include "interfaces/ISystemInfo.h"

class MockSystemInfo : public ISystemInfo {
public:
    int rssi = -55;
    uint32_t freeHeap = 150 * 1024;
    uint32_t maxAlloc = 100 * 1024;
    uint32_t minFreeHeap = 80 * 1024;
    const char* reason = "POWERON";
    uint32_t reconnects = 0;

    int wifiRssi() override { return rssi; }
    uint32_t freeHeapBytes() override { return freeHeap; }
    uint32_t maxAllocBytes() override { return maxAlloc; }
    uint32_t minFreeHeapBytes() override { return minFreeHeap; }
    const char* resetReason() override { return reason; }
    void noteWifiReconnect() override { reconnects++; }
    uint32_t wifiReconnectCount() override { return reconnects; }
};
