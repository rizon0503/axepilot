#include "hal/EspSystemInfo.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

int EspSystemInfo::wifiRssi() {
    return WiFi.RSSI();
}

uint32_t EspSystemInfo::freeHeapBytes() {
    return ESP.getFreeHeap();
}

uint32_t EspSystemInfo::maxAllocBytes() {
    return ESP.getMaxAllocHeap();
}

uint32_t EspSystemInfo::minFreeHeapBytes() {
    return esp_get_minimum_free_heap_size();
}

const char* EspSystemInfo::resetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_SW:        return "SOFTWARE";
        case ESP_RST_PANIC:     return "PANIC (crash!)";
        case ESP_RST_INT_WDT:   return "INT WATCHDOG";
        case ESP_RST_TASK_WDT:  return "TASK WATCHDOG";
        case ESP_RST_WDT:       return "WATCHDOG";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP WAKE";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (power!)";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

void EspSystemInfo::noteWifiReconnect() {
    wifiReconnects++;
}

uint32_t EspSystemInfo::wifiReconnectCount() {
    return wifiReconnects.load();
}
