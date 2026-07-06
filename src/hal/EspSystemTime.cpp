#include "hal/EspSystemTime.h"
#include <Arduino.h>

uint32_t EspSystemTime::millis() {
    return ::millis();
}

void EspSystemTime::delay(uint32_t ms) {
    ::delay(ms);
}

time_t EspSystemTime::epochSeconds() {
    time_t now;
    ::time(&now);
    // Before NTP syncs, the ESP32's clock starts near epoch 0 (1970) —
    // treat anything before a sane baseline (2020-01-01 UTC) as "not
    // synced yet" rather than a real, very-out-of-date timestamp.
    return (now > 1577836800) ? now : 0;
}
