#pragma once
#include <stdint.h>

class ISystemTime {
public:
    virtual ~ISystemTime() = default;
    virtual uint32_t millis() = 0;
    virtual void delay(uint32_t ms) = 0;
};
