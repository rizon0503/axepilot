#pragma once
#include "interfaces/ISystemTime.h"

class MockSystemTime : public ISystemTime {
public:
    uint32_t currentTime = 0;
    uint32_t millis() override { return currentTime; }
    void delay(uint32_t ms) override { currentTime += ms; }
};
