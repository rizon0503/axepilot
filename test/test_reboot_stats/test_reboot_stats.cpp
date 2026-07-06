#include <unity.h>
#include "core/RebootStats.h"
#include "../mocks/MockSettingsStore.h"

void setUp(void) {}
void tearDown(void) {}

void test_reset_count_increments_and_saves_on_construction() {
    MockSettingsStore store;
    store.storedResetCount = 5;

    RebootStats stats(store);
    stats.begin();

    TEST_ASSERT_EQUAL(6, stats.resetCount());
    TEST_ASSERT_EQUAL(6, store.storedResetCount);
    TEST_ASSERT_EQUAL(1, store.saveResetCountCalls);
}

void test_uptime_record_persists_once_past_threshold() {
    MockSettingsStore store;
    store.storedUptimeRecord = 100;
    RebootStats stats(store);
    stats.begin();

    stats.tick(150, 0); // only +50, below the 60s save threshold
    TEST_ASSERT_EQUAL(100, stats.uptimeRecordSeconds());
    TEST_ASSERT_EQUAL(0, store.saveUptimeRecordCalls);

    stats.tick(170, 0); // now +70 past the original record -> saves
    TEST_ASSERT_EQUAL(170, stats.uptimeRecordSeconds());
    TEST_ASSERT_EQUAL(1, store.saveUptimeRecordCalls);
}

void test_uptime_record_never_regresses_on_a_short_session() {
    MockSettingsStore store;
    store.storedUptimeRecord = 5000; // a long previous session
    RebootStats stats(store);
    stats.begin();

    stats.tick(30, 0); // this boot has only been up 30s
    TEST_ASSERT_EQUAL(5000, stats.uptimeRecordSeconds());
    TEST_ASSERT_EQUAL(0, store.saveUptimeRecordCalls);
}

void test_intervention_total_saved_only_on_change() {
    MockSettingsStore store;
    store.storedInterventionTotal = 3;
    RebootStats stats(store);
    stats.begin();

    stats.tick(0, 3); // unchanged
    TEST_ASSERT_EQUAL(0, store.saveInterventionTotalCalls);

    stats.tick(0, 4); // changed
    TEST_ASSERT_EQUAL(4, stats.interventionTotal());
    TEST_ASSERT_EQUAL(1, store.saveInterventionTotalCalls);

    stats.tick(0, 4); // unchanged again
    TEST_ASSERT_EQUAL(1, store.saveInterventionTotalCalls);
}

void test_first_boot_starts_from_zero() {
    MockSettingsStore store; // all defaults 0

    RebootStats stats(store);
    stats.begin();

    TEST_ASSERT_EQUAL(1, stats.resetCount());
    TEST_ASSERT_EQUAL(0, stats.uptimeRecordSeconds());
    TEST_ASSERT_EQUAL(0, stats.interventionTotal());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_count_increments_and_saves_on_construction);
    RUN_TEST(test_uptime_record_persists_once_past_threshold);
    RUN_TEST(test_uptime_record_never_regresses_on_a_short_session);
    RUN_TEST(test_intervention_total_saved_only_on_change);
    RUN_TEST(test_first_boot_starts_from_zero);
    return UNITY_END();
}
