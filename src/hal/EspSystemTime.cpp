#include "hal/EspSystemTime.h"
#include <Arduino.h>

uint32_t EspSystemTime::millis() {
    return ::millis();
}

void EspSystemTime::delay(uint32_t ms) {
    ::delay(ms);
}
