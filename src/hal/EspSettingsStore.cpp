#include "hal/EspSettingsStore.h"
#include <Preferences.h>

namespace {
constexpr const char* NAMESPACE_NAME = "axepilot";
constexpr const char* MODE_KEY = "mode";
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
