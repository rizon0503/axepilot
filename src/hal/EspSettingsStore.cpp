#include "hal/EspSettingsStore.h"
#include <Preferences.h>

namespace {
constexpr const char* NAMESPACE_NAME = "axepilot";
constexpr const char* MODE_KEY = "mode";
constexpr const char* RESET_COUNT_KEY = "resets";
constexpr const char* UPTIME_RECORD_KEY = "uptimeRec";
constexpr const char* INTERVENTION_TOTAL_KEY = "aiTotal";
}

OperationMode EspSettingsStore::loadMode(OperationMode fallback) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/true)) {
        return fallback; // namespace does not exist yet (first boot)
    }
    uint8_t stored = prefs.getUChar(MODE_KEY, static_cast<uint8_t>(fallback));
    prefs.end();
    return stored == static_cast<uint8_t>(OperationMode::MANUAL)
               ? OperationMode::MANUAL
               : OperationMode::AUTOPILOT;
}

void EspSettingsStore::saveMode(OperationMode mode) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/false)) {
        return;
    }
    prefs.putUChar(MODE_KEY, static_cast<uint8_t>(mode));
    prefs.end();
}

uint32_t EspSettingsStore::loadResetCount() {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/true)) {
        return 0;
    }
    uint32_t count = prefs.getUInt(RESET_COUNT_KEY, 0);
    prefs.end();
    return count;
}

void EspSettingsStore::saveResetCount(uint32_t count) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/false)) {
        return;
    }
    prefs.putUInt(RESET_COUNT_KEY, count);
    prefs.end();
}

uint32_t EspSettingsStore::loadUptimeRecordSeconds() {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/true)) {
        return 0;
    }
    uint32_t seconds = prefs.getUInt(UPTIME_RECORD_KEY, 0);
    prefs.end();
    return seconds;
}

void EspSettingsStore::saveUptimeRecordSeconds(uint32_t seconds) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/false)) {
        return;
    }
    prefs.putUInt(UPTIME_RECORD_KEY, seconds);
    prefs.end();
}

uint32_t EspSettingsStore::loadInterventionTotal() {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/true)) {
        return 0;
    }
    uint32_t total = prefs.getUInt(INTERVENTION_TOTAL_KEY, 0);
    prefs.end();
    return total;
}

void EspSettingsStore::saveInterventionTotal(uint32_t total) {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE_NAME, /*readOnly=*/false)) {
        return;
    }
    prefs.putUInt(INTERVENTION_TOTAL_KEY, total);
    prefs.end();
}
