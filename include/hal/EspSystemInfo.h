#pragma once
#include "interfaces/ISystemInfo.h"
#include <atomic>

class EspSystemInfo : public ISystemInfo {
public:
    int wifiRssi() override;
    uint32_t freeHeapBytes() override;
    uint32_t maxAllocBytes() override;
    uint32_t minFreeHeapBytes() override;
    const char* resetReason() override;
    void noteWifiReconnect() override;
    uint32_t wifiReconnectCount() override;

private:
    std::atomic<uint32_t> wifiReconnects{0};
};
