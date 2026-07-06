#pragma once
#include "interfaces/ISystemTime.h"

class MockSystemTime : public ISystemTime {
public:
    uint32_t currentTime = 0;
    time_t currentEpoch = 0; // 0 = "not synced", matching EspSystemTime's convention
    uint32_t millis() override { return currentTime; }
    void delay(uint32_t ms) override { currentTime += ms; }
    time_t epochSeconds() override { return currentEpoch; }
};
