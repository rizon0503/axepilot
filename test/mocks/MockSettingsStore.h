#pragma once
#include "interfaces/ISettingsStore.h"

class MockSettingsStore : public ISettingsStore {
public:
    OperationMode storedMode = OperationMode::AUTOPILOT;
    uint32_t storedResetCount = 0;
    uint32_t storedUptimeRecord = 0;
    uint32_t storedInterventionTotal = 0;

    int saveResetCountCalls = 0;
    int saveUptimeRecordCalls = 0;
    int saveInterventionTotalCalls = 0;

    OperationMode loadMode(OperationMode /*fallback*/) override { return storedMode; }
    void saveMode(OperationMode mode) override { storedMode = mode; }

    uint32_t loadResetCount() override { return storedResetCount; }
    void saveResetCount(uint32_t count) override {
        storedResetCount = count;
        saveResetCountCalls++;
    }

    uint32_t loadUptimeRecordSeconds() override { return storedUptimeRecord; }
    void saveUptimeRecordSeconds(uint32_t seconds) override {
        storedUptimeRecord = seconds;
        saveUptimeRecordCalls++;
    }

    uint32_t loadInterventionTotal() override { return storedInterventionTotal; }
    void saveInterventionTotal(uint32_t total) override {
        storedInterventionTotal = total;
        saveInterventionTotalCalls++;
    }
};
