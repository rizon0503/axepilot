#pragma once
#include "interfaces/ISettingsStore.h"

class EspSettingsStore : public ISettingsStore {
public:
    OperationMode loadMode(OperationMode fallback) override;
    void saveMode(OperationMode mode) override;

    uint32_t loadResetCount() override;
    void saveResetCount(uint32_t count) override;
    uint32_t loadUptimeRecordSeconds() override;
    void saveUptimeRecordSeconds(uint32_t seconds) override;
    uint32_t loadInterventionTotal() override;
    void saveInterventionTotal(uint32_t total) override;
};
