#pragma once
#include "interfaces/ISystemTime.h"

class EspSystemTime : public ISystemTime {
public:
    uint32_t millis() override;
    void delay(uint32_t ms) override;
};
