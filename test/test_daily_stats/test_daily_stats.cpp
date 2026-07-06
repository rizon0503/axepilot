#include <unity.h>
#include "core/DailyStats.h"
#include <string>

void setUp(void) {}
void tearDown(void) {}

static BitaxeData sample(float temp, float hash, float power, uint32_t shares, uint32_t rejected) {
    BitaxeData data{};
    data.temperature = temp;
    data.hashrate = hash;
    data.power = power;
    data.sharesAccepted = shares;
    data.sharesRejected = rejected;
    return data;
}

void test_no_digest_before_period_ends() {
    DailyStats stats;

    TEST_ASSERT_EQUAL(0, stats.tick(0, 0, 0).length()); // starts the period
    stats.record(sample(60.0f, 1000.0f, 15.0f, 100, 1), 1000);

    std::string digest = stats.tick(DailyStats::PERIOD_MS - 1000, 0, 2);
    TEST_ASSERT_EQUAL(0, digest.length());
}

void test_digest_after_24h_contains_stats() {
    DailyStats stats;
    stats.tick(0, 0, 2); // period starts, 2 interventions so far

    stats.record(sample(55.0f, 1000.0f, 14.0f, 100, 1), 1000);
    stats.record(sample(65.0f, 1200.0f, 16.0f, 5100, 13), 2000);

    std::string digest = stats.tick(DailyStats::PERIOD_MS + 1000, 0, 5);

    TEST_ASSERT_TRUE(digest.find("60.0") != std::string::npos);   // avg temp
    TEST_ASSERT_TRUE(digest.find("55.0") != std::string::npos);   // min temp
    TEST_ASSERT_TRUE(digest.find("65.0") != std::string::npos);   // max temp
    TEST_ASSERT_TRUE(digest.find("1100") != std::string::npos);   // avg hashrate
    TEST_ASSERT_TRUE(digest.find("+5000") != std::string::npos);  // shares delta
    TEST_ASSERT_TRUE(digest.find("12") != std::string::npos);     // rejected delta
    TEST_ASSERT_TRUE(digest.find("73.3 GH/W") != std::string::npos); // 1100/15
    TEST_ASSERT_TRUE(digest.find("3") != std::string::npos);      // 5-2 interventions
}

void test_second_period_starts_fresh() {
    DailyStats stats;
    stats.tick(0, 0, 0);
    stats.record(sample(55.0f, 1000.0f, 14.0f, 100, 0), 1000);
    stats.tick(DailyStats::PERIOD_MS + 1000, 0, 4); // first digest, resets

    // New period gets different data
    stats.record(sample(70.0f, 800.0f, 20.0f, 6000, 5), DailyStats::PERIOD_MS + 5000);
    std::string digest = stats.tick(2 * DailyStats::PERIOD_MS + 2000, 0, 4);

    TEST_ASSERT_TRUE(digest.find("70.0") != std::string::npos);
    TEST_ASSERT_TRUE(digest.find("55.0") == std::string::npos);   // old data gone
    TEST_ASSERT_TRUE(digest.find("Interventions today: 0") != std::string::npos);
}

void test_digest_handles_day_without_telemetry() {
    DailyStats stats;
    stats.tick(0, 0, 0);

    std::string digest = stats.tick(DailyStats::PERIOD_MS + 1000, 0, 0);

    TEST_ASSERT_TRUE(digest.find("no valid telemetry") != std::string::npos);
}

void test_ntp_digest_does_not_fire_before_configured_hour() {
    DailyStats stats;
    // 2025-01-01T00:00:00Z (boot instant, establishes the baseline day)
    time_t boot = 1735689600;
    stats.tick(0, boot, 0);
    stats.record(sample(60.0f, 1000.0f, 15.0f, 100, 1), 1000);

    // Same day, 08:59 UTC — before the 09:00 digest hour
    std::string digest = stats.tick(1000, boot + 8 * 3600 + 59 * 60, 1);
    TEST_ASSERT_EQUAL(0, digest.length());
}

void test_ntp_digest_fires_once_past_configured_hour() {
    DailyStats stats;
    time_t boot = 1735689600; // 2025-01-01T00:00:00Z
    stats.tick(0, boot, 0);
    stats.record(sample(60.0f, 1000.0f, 15.0f, 100, 1), 1000);

    // Same day, 09:00 UTC — the configured digest hour
    std::string digest = stats.tick(1000, boot + 9 * 3600, 1);
    TEST_ASSERT_TRUE(digest.length() > 0);
    TEST_ASSERT_TRUE(digest.find("60.0") != std::string::npos);
}

void test_ntp_digest_fires_only_once_per_day() {
    DailyStats stats;
    time_t boot = 1735689600;
    stats.tick(0, boot, 0);
    stats.record(sample(60.0f, 1000.0f, 15.0f, 100, 1), 1000);

    stats.tick(1000, boot + 9 * 3600, 1); // fires at 09:00
    stats.record(sample(62.0f, 1000.0f, 15.0f, 100, 1), 2000);

    // Later the same day, still after 09:00 — must not fire again
    std::string digest = stats.tick(2000, boot + 15 * 3600, 1);
    TEST_ASSERT_EQUAL(0, digest.length());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_no_digest_before_period_ends);
    RUN_TEST(test_digest_after_24h_contains_stats);
    RUN_TEST(test_second_period_starts_fresh);
    RUN_TEST(test_digest_handles_day_without_telemetry);
    RUN_TEST(test_ntp_digest_does_not_fire_before_configured_hour);
    RUN_TEST(test_ntp_digest_fires_once_past_configured_hour);
    RUN_TEST(test_ntp_digest_fires_only_once_per_day);
    return UNITY_END();
}
