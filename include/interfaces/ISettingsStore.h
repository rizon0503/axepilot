#pragma once
#include "core/OperationMode.h"
#include <cstdint>

// Persistent user settings that must survive a reboot / power loss.
class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;
    virtual OperationMode loadMode(OperationMode fallback) = 0;
    virtual void saveMode(OperationMode mode) = 0;

    // Reboot-surviving counters (see core/RebootStats), exposed via /esp.
    virtual uint32_t loadResetCount() = 0;
    virtual void saveResetCount(uint32_t count) = 0;
    virtual uint32_t loadUptimeRecordSeconds() = 0;
    virtual void saveUptimeRecordSeconds(uint32_t seconds) = 0;
    virtual uint32_t loadInterventionTotal() = 0;
    virtual void saveInterventionTotal(uint32_t total) = 0;
};
