#include <unity.h>
#include "core/TelemetryHistory.h"
#include <string>
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

static BitaxeData sample(float temp, float hash, float power = 0.0f) {
    BitaxeData data{};
    data.temperature = temp;
    data.hashrate = hash;
    data.power = power;
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

void test_history_computes_average_power() {
    TelemetryHistory history;

    history.record(sample(60.0f, 1000.0f, 14.0f), 0);
    history.record(sample(62.0f, 1200.0f, 16.0f), 30000);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.0f, history.avgPower());
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

void test_summary_contains_average_efficiency() {
    TelemetryHistory history;
    history.record(sample(60.0f, 1000.0f, 14.0f), 0);
    history.record(sample(62.0f, 1200.0f, 16.0f), 120000);
    // avg hashrate 1100, avg power 15 -> 73.3 GH/W

    char buf[256];
    history.summarize(buf, sizeof(buf));

    std::string summary(buf);
    TEST_ASSERT_TRUE(summary.find("73.3 GH/W") != std::string::npos);
}

void test_sparkline_data_is_empty_with_no_samples() {
    TelemetryHistory history;

    float temps[TelemetryHistory::CAPACITY];
    float hashrates[TelemetryHistory::CAPACITY];
    size_t n = history.copySparklineData(temps, hashrates, TelemetryHistory::CAPACITY);

    TEST_ASSERT_EQUAL(0, n);
}

void test_sparkline_data_is_oldest_first() {
    TelemetryHistory history;
    history.record(sample(60.0f, 1000.0f), 0);
    history.record(sample(65.0f, 1200.0f), 30000);
    history.record(sample(70.0f, 1400.0f), 60000);

    float temps[TelemetryHistory::CAPACITY];
    float hashrates[TelemetryHistory::CAPACITY];
    size_t n = history.copySparklineData(temps, hashrates, TelemetryHistory::CAPACITY);

    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, temps[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, temps[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 70.0f, temps[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, hashrates[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1400.0f, hashrates[2]);
}

void test_sparkline_data_respects_wraparound() {
    TelemetryHistory history;
    for (int i = 0; i < 25; i++) {
        history.record(sample(50.0f + i, 1000.0f), (uint32_t)i * 30000);
    }

    float temps[TelemetryHistory::CAPACITY];
    float hashrates[TelemetryHistory::CAPACITY];
    size_t n = history.copySparklineData(temps, hashrates, TelemetryHistory::CAPACITY);

    TEST_ASSERT_EQUAL(TelemetryHistory::CAPACITY, n);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f, temps[0]);  // oldest surviving (i=5)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 74.0f, temps[19]); // newest (i=24)
}

void test_sparkline_data_respects_caller_buffer_limit() {
    TelemetryHistory history;
    history.record(sample(60.0f, 1000.0f), 0);
    history.record(sample(65.0f, 1200.0f), 30000);
    history.record(sample(70.0f, 1400.0f), 60000);

    float temps[2];
    float hashrates[2];
    size_t n = history.copySparklineData(temps, hashrates, 2);

    // Truncates to the buffer size, keeping the most recent samples.
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, temps[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 70.0f, temps[1]);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_history_rate_limits_recording);
    RUN_TEST(test_history_ignores_invalid_temperature);
    RUN_TEST(test_history_computes_rising_trend);
    RUN_TEST(test_history_computes_averages);
    RUN_TEST(test_history_computes_average_power);
    RUN_TEST(test_history_wraps_at_capacity);
    RUN_TEST(test_summary_empty_with_too_few_samples);
    RUN_TEST(test_summary_contains_trend);
    RUN_TEST(test_summary_contains_average_efficiency);
    RUN_TEST(test_sparkline_data_is_empty_with_no_samples);
    RUN_TEST(test_sparkline_data_is_oldest_first);
    RUN_TEST(test_sparkline_data_respects_wraparound);
    RUN_TEST(test_sparkline_data_respects_caller_buffer_limit);
    return UNITY_END();
}
