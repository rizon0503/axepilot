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
    MockSettingsStore store; // no interventions from previous boots
    RebootStats stats(store);
    stats.begin();

    stats.tick(0, 0); // unchanged (still 0 interventions this boot)
    TEST_ASSERT_EQUAL(0, store.saveInterventionTotalCalls);

    stats.tick(0, 1); // one intervention happened this boot
    TEST_ASSERT_EQUAL(1, stats.interventionTotal());
    TEST_ASSERT_EQUAL(1, store.saveInterventionTotalCalls);

    stats.tick(0, 1); // unchanged again
    TEST_ASSERT_EQUAL(1, store.saveInterventionTotalCalls);
}

void test_intervention_total_accumulates_across_reboots_instead_of_resetting() {
    // InterventionLog::totalCount() always starts back at 0 after a
    // reboot (it's in-memory only) — RebootStats exists precisely to
    // remember the lifetime total across that reset, so it must not be
    // clobbered by this boot's fresh, smaller in-memory count.
    MockSettingsStore store;
    store.storedInterventionTotal = 5; // lifetime total from previous boots

    RebootStats stats(store);
    stats.begin();

    stats.tick(0, 0); // no interventions yet this boot
    TEST_ASSERT_EQUAL(5, stats.interventionTotal());

    stats.tick(0, 2); // 2 interventions happened this boot
    TEST_ASSERT_EQUAL(7, stats.interventionTotal()); // 5 (previous) + 2 (this boot)
    TEST_ASSERT_EQUAL(7, store.storedInterventionTotal);
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
    RUN_TEST(test_intervention_total_accumulates_across_reboots_instead_of_resetting);
    RUN_TEST(test_first_boot_starts_from_zero);
    return UNITY_END();
}
