#include <unity.h>
#include "core/InterventionLog.h"
#include "core/BitaxeController.h"
#include "../mocks/MockHttpClient.h"
#include "../mocks/MockSystemTime.h"
#include <string>

void setUp(void) {}
void tearDown(void) {}

void test_log_records_and_formats_newest_first() {
    InterventionLog log;
    log.add(10000, 0, "Autopilot", 490, 1100);
    log.add(70000, 0, "/set", 550, 1150);

    char buf[512];
    log.format(buf, sizeof(buf), 130000); // 2 min after first, 1 min after second

    std::string out(buf);
    size_t posSet = out.find("/set");
    size_t posAuto = out.find("Autopilot");
    TEST_ASSERT_TRUE(posSet != std::string::npos);
    TEST_ASSERT_TRUE(posAuto != std::string::npos);
    TEST_ASSERT_TRUE(posSet < posAuto); // newest entry first
    TEST_ASSERT_TRUE(out.find("550 MHz / 1150 mV") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("1m ago") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("2m ago") != std::string::npos);
}

void test_log_wraps_at_capacity() {
    InterventionLog log;
    for (int i = 0; i < 15; i++) {
        log.add((uint32_t)i * 1000, 0, "src", 400 + i, 1000);
    }

    TEST_ASSERT_EQUAL(InterventionLog::CAPACITY, log.size());

    char buf[1024];
    log.format(buf, sizeof(buf), 20000);
    std::string out(buf);
    TEST_ASSERT_TRUE(out.find("414 MHz") != std::string::npos);  // newest kept
    TEST_ASSERT_TRUE(out.find("404 MHz") == std::string::npos);  // oldest evicted
}

void test_log_shows_real_timestamp_once_ntp_synced() {
    InterventionLog log;
    // 2025-01-01T00:00:00Z
    log.add(10000, 1735689600, "Autopilot", 490, 1100);

    char buf[256];
    log.format(buf, sizeof(buf), 20000);

    std::string out(buf);
    TEST_ASSERT_TRUE(out.find("00:00 UTC") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("ago") == std::string::npos);
}

void test_log_mixes_relative_and_real_timestamps() {
    InterventionLog log;
    log.add(10000, 0, "Boot fallback", 400, 1000);       // recorded before NTP synced
    log.add(70000, 1735689600, "Autopilot", 490, 1100);  // recorded after NTP synced

    char buf[512];
    log.format(buf, sizeof(buf), 130000);

    std::string out(buf);
    TEST_ASSERT_TRUE(out.find("00:00 UTC") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("2m ago") != std::string::npos);
}

void test_controller_apply_settings_records_intervention() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");

    mockTime.currentTime = 42000;
    miner.applySettings(525, 1100, "Test");

    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(1, miner.interventions().size());

    char buf[256];
    miner.interventions().format(buf, sizeof(buf), 42000);
    std::string out(buf);
    TEST_ASSERT_TRUE(out.find("Test") != std::string::npos);
    TEST_ASSERT_TRUE(out.find("525 MHz / 1100 mV") != std::string::npos);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_log_records_and_formats_newest_first);
    RUN_TEST(test_log_wraps_at_capacity);
    RUN_TEST(test_log_shows_real_timestamp_once_ntp_synced);
    RUN_TEST(test_log_mixes_relative_and_real_timestamps);
    RUN_TEST(test_controller_apply_settings_records_intervention);
    return UNITY_END();
}
