#include <unity.h>
#include "core/TelemetryHistory.h"
#include <string>
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

static BitaxeData sample(float temp, float hash) {
    BitaxeData data{};
    data.temperature = temp;
    data.hashrate = hash;
    return data;
}

void test_history_rate_limits_recording() {
    TelemetryHistory history;

    history.record(sample(60.0f, 1000.0f), 0);
    history.record(sample(61.0f, 1000.0f), 10000);  // 10 s later — ignored
    history.record(sample(62.0f, 1000.0f), 29000);  // still inside 30 s — ignored
    history.record(sample(63.0f, 1000.0f), 30000);  // accepted

    TEST_ASSERT_EQUAL(2, history.size());
}

void test_history_ignores_invalid_temperature() {
    TelemetryHistory history;

    history.record(sample(0.0f, 1000.0f), 0);
    history.record(sample(-1.0f, 1000.0f), 40000);

    TEST_ASSERT_EQUAL(0, history.size());
}

void test_history_computes_rising_trend() {
    TelemetryHistory history;

    // +1°C every 30 s = +2°C per minute
    history.record(sample(60.0f, 1000.0f), 0);
    history.record(sample(61.0f, 1000.0f), 30000);
    history.record(sample(62.0f, 1000.0f), 60000);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, history.tempTrendPerMinute());
}

void test_history_computes_averages() {
    TelemetryHistory history;

    history.record(sample(60.0f, 1000.0f), 0);
    history.record(sample(62.0f, 1200.0f), 30000);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 61.0f, history.avgTemperature());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1100.0f, history.avgHashrate());
}

void test_history_wraps_at_capacity() {
    TelemetryHistory history;

    // Fill 25 samples into 20 slots: oldest 5 must be evicted
    for (int i = 0; i < 25; i++) {
        history.record(sample(50.0f + i, 1000.0f), (uint32_t)i * 30000);
    }

    TEST_ASSERT_EQUAL(TelemetryHistory::CAPACITY, history.size());
    // Oldest surviving sample is i=5 (55.0), newest i=24 (74.0):
    // trend = 19°C over 19*30s = 2°C/min
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, history.tempTrendPerMinute());
}

void test_summary_empty_with_too_few_samples() {
    TelemetryHistory history;
    history.record(sample(60.0f, 1000.0f), 0);

    char buf[256];
    history.summarize(buf, sizeof(buf));

    TEST_ASSERT_EQUAL(0, strlen(buf));
}

void test_summary_contains_trend() {
    TelemetryHistory history;
    history.record(sample(60.0f, 1000.0f), 0);
    history.record(sample(64.0f, 1100.0f), 120000); // +4°C over 2 min = +2/min

    char buf[256];
    history.summarize(buf, sizeof(buf));

    std::string summary(buf);
    TEST_ASSERT_TRUE(summary.find("+2.00C/min") != std::string::npos);
    TEST_ASSERT_TRUE(summary.find("2 samples") != std::string::npos);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_history_rate_limits_recording);
    RUN_TEST(test_history_ignores_invalid_temperature);
    RUN_TEST(test_history_computes_rising_trend);
    RUN_TEST(test_history_computes_averages);
    RUN_TEST(test_history_wraps_at_capacity);
    RUN_TEST(test_summary_empty_with_too_few_samples);
    RUN_TEST(test_summary_contains_trend);
    return UNITY_END();
}
