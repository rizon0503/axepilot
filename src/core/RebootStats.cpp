#include "core/RebootStats.h"

RebootStats::RebootStats(ISettingsStore& store) : store(store) {}

void RebootStats::begin() {
    resetCountValue = store.loadResetCount() + 1;
    store.saveResetCount(resetCountValue);

    uptimeRecordValue = store.loadUptimeRecordSeconds();
    lastSavedInterventionTotal = store.loadInterventionTotal();
    interventionBaselineAtBoot = lastSavedInterventionTotal;
    lastSeenCurrentBootTotal = 0;
}

void RebootStats::tick(uint32_t uptimeSeconds, uint32_t currentInterventionTotal) {
    if (uptimeSeconds >= uptimeRecordValue + UPTIME_SAVE_THRESHOLD_SECONDS) {
        uptimeRecordValue = uptimeSeconds;
        store.saveUptimeRecordSeconds(uptimeRecordValue);
    }
    if (currentInterventionTotal != lastSeenCurrentBootTotal) {
        lastSeenCurrentBootTotal = currentInterventionTotal;
        lastSavedInterventionTotal = interventionBaselineAtBoot + currentInterventionTotal;
        store.saveInterventionTotal(lastSavedInterventionTotal);
    }
}
