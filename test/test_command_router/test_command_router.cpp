#include <unity.h>
#include <cstdio>
#include "core/CommandRouter.h"
#include "core/Limits.h"
#include "../mocks/MockHttpClient.h"
#include "../mocks/MockSystemTime.h"
#include "../mocks/MockSystemInfo.h"
#include "../mocks/MockSettingsStore.h"

void setUp(void) {}
void tearDown(void) {}

static std::string deepseekReply(const std::string& innerJson) {
    std::string escaped;
    for (char c : innerJson) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }
    return "{\"choices\":[{\"message\":{\"content\":\"" + escaped + "\"}}]}";
}

// Everything the router needs, wired to shared mocks
struct Fixture {
    MockHttpClient http;
    MockSystemTime time;
    MockSystemInfo info;
    MockSettingsStore settingsStore;
    BitaxeController miner;
    TelegramNotifier notifier;
    DeepSeekOptimizer optimizer;
    TelemetryHistory history;
    BenchmarkRunner benchmark;
    RebootStats rebootStats;
    CommandRouter router;
    BitaxeData data{};

    Fixture()
        : miner(http, time, "192.168.0.128"),
          notifier(http, "dummy_token", "12345"),
          optimizer(http, time, "dummy_key", miner),
          benchmark(miner),
          rebootStats(settingsStore),
          router(notifier, optimizer, miner, info, time, history, benchmark, rebootStats) {
        rebootStats.begin();
        data.temperature = 62.5f;
        data.hashrate = 1150.0f;
        data.frequency = 550;
        data.coreVoltage = 1150;
        data.power = 15.0f;
        data.fanSpeedPercent = 82;
        data.sharesAccepted = 1234;
        data.sharesRejected = 5;
        snprintf(data.bestDiff, sizeof(data.bestDiff), "43.9M");
    }
};

void test_auto_command_switches_mode() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/auto", f.data, mode);

    TEST_ASSERT_TRUE(mode == OperationMode::AUTOPILOT);
    TEST_ASSERT_EQUAL(1, f.http.postCount); // confirmation sent
}

void test_manual_command_switches_mode() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/manual", f.data, mode);

    TEST_ASSERT_TRUE(mode == OperationMode::MANUAL);
}

void test_status_reports_telemetry() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/status", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("62.5") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("AUTOPILOT") != std::string::npos);
    // Extended telemetry: efficiency (1150/15 = 76.7 GH/W), fan, shares, bestDiff
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("76.7") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("82%") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("1234") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("43.9M") != std::string::npos);
}

void test_status_shows_vr_temp_when_reported() {
    Fixture f;
    f.data.vrTemp = 81.3f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/status", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("VR Temp: 81.3") != std::string::npos);
}

void test_status_omits_vr_temp_when_unsupported() {
    Fixture f;
    f.data.vrTemp = 0.0f; // default already, but explicit for clarity
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/status", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("VR Temp") == std::string::npos);
}

void test_status_shows_pool_info_when_reported() {
    Fixture f;
    snprintf(f.data.stratumURL, sizeof(f.data.stratumURL), "public-pool.io");
    f.data.stratumPort = 21496;
    snprintf(f.data.stratumUser, sizeof(f.data.stratumUser), "bc1qxyz.worker1");
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/status", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("Pool: public-pool.io:21496 (bc1qxyz.worker1)") != std::string::npos);
    // Must be appended after the rest of the status, not overwriting it
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("AUTOPILOT") != std::string::npos);
}

void test_status_omits_pool_info_when_not_configured() {
    Fixture f; // stratumURL defaults to empty
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/status", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("Pool:") == std::string::npos);
}

void test_status_shows_auto_fan_with_rpm_when_percent_is_zero() {
    Fixture f;
    f.data.fanSpeedPercent = 0; // AxeOS autofanspeed mode reports 0%
    f.data.fanRpm = 4800;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/status", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("4800") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("0%") == std::string::npos);
}

void test_esp_reports_system_info() {
    Fixture f;
    f.info.rssi = -61;
    f.info.reason = "BROWNOUT (power!)";
    f.info.minFreeHeap = 64 * 1024;
    f.info.noteWifiReconnect();
    f.info.noteWifiReconnect();
    f.time.currentTime = 3600 * 1000; // 1 hour uptime
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/esp", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("-61") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("01:00:00") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("BROWNOUT") != std::string::npos);
    // Reboot-surviving counters: fresh fixture -> this is reset #1, no
    // uptime record yet, no interventions yet
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("resets so far: 1") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("record: 00:00:00") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("interventions (lifetime): 0") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("64 KB") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("2") != std::string::npos);
}

void test_esp_reports_preloaded_reboot_stats() {
    MockHttpClient http;
    MockSystemTime time;
    MockSystemInfo info;
    MockSettingsStore settingsStore;
    settingsStore.storedResetCount = 9; // this boot will become #10
    settingsStore.storedUptimeRecord = 7325; // 02:02:05
    settingsStore.storedInterventionTotal = 42;
    BitaxeController miner(http, time, "192.168.0.128");
    TelegramNotifier notifier(http, "dummy_token", "12345");
    DeepSeekOptimizer optimizer(http, time, "dummy_key", miner);
    TelemetryHistory history;
    BenchmarkRunner benchmark(miner);
    RebootStats rebootStats(settingsStore);
    rebootStats.begin();
    CommandRouter router(notifier, optimizer, miner, info, time, history, benchmark, rebootStats);
    BitaxeData data{};
    OperationMode mode = OperationMode::MANUAL;

    router.handle("/esp", data, mode);

    TEST_ASSERT_TRUE(http.lastPostPayload.find("resets so far: 10") != std::string::npos);
    TEST_ASSERT_TRUE(http.lastPostPayload.find("record: 02:02:05") != std::string::npos);
    TEST_ASSERT_TRUE(http.lastPostPayload.find("interventions (lifetime): 42") != std::string::npos);
}

void test_set_applies_valid_settings_and_forces_manual() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/set 550 1150", f.data, mode);

    TEST_ASSERT_TRUE(mode == OperationMode::MANUAL);
    TEST_ASSERT_EQUAL(1, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPatchPayload.find("\"frequency\":550") != std::string::npos);
}

void test_set_rejects_out_of_range_settings() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/set 900 1500", f.data, mode);

    TEST_ASSERT_TRUE(mode == OperationMode::AUTOPILOT); // mode unchanged
    TEST_ASSERT_EQUAL(0, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("Invalid values") != std::string::npos);
}

void test_set_rejects_malformed_input() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/set banana", f.data, mode);

    TEST_ASSERT_EQUAL(0, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("Invalid format") != std::string::npos);
}

void test_fan_manual_speed_within_range() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/fan 60", f.data, mode);

    TEST_ASSERT_EQUAL(1, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPatchPayload.find("\"autofanspeed\":0") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPatchPayload.find("\"fanspeed\":60") != std::string::npos);
}

void test_fan_rejects_dangerously_low_speed() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/fan 10", f.data, mode);

    TEST_ASSERT_EQUAL(0, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("25-100") != std::string::npos);
}

void test_fan_auto_restores_autofanspeed() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/fan auto", f.data, mode);

    TEST_ASSERT_EQUAL(1, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPatchPayload.find("\"autofanspeed\":1") != std::string::npos);
}

void test_restart_posts_to_miner() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/restart", f.data, mode);

    TEST_ASSERT_TRUE(f.http.postedTo("/api/system/restart"));
}

void test_history_empty_message() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/history", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("empty") != std::string::npos);
}

void test_history_lists_interventions_after_set() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/set 550 1150", f.data, mode);
    f.router.handle("/history", f.data, mode);

    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("/set") != std::string::npos);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("550 MHz / 1150 mV") != std::string::npos);
}

void test_bench_start_forces_manual_and_applies_first_preset() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    f.router.handle("/bench start", f.data, mode);

    TEST_ASSERT_TRUE(mode == OperationMode::MANUAL);
    TEST_ASSERT_TRUE(f.benchmark.isRunning());
    TEST_ASSERT_EQUAL(1, f.http.patchCount);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("Benchmark started") != std::string::npos);
}

void test_bench_status_and_stop() {
    Fixture f;
    OperationMode mode = OperationMode::MANUAL;

    f.router.handle("/bench", f.data, mode);
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("not running") != std::string::npos);

    f.router.handle("/bench start", f.data, mode);
    f.router.handle("/bench stop", f.data, mode);
    TEST_ASSERT_FALSE(f.benchmark.isRunning());
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("stopped") != std::string::npos);
}

void test_free_text_goes_to_ai_and_mode_change_is_applied() {
    Fixture f;
    OperationMode mode = OperationMode::AUTOPILOT;

    // AI answers with a settings change -> router must force MANUAL
    f.http.postResponse = deepseekReply("{\"reply\":\"Setting 490\",\"frequency\":490,\"coreVoltage\":1100}");

    f.router.handle("set it to 490 please", f.data, mode);

    TEST_ASSERT_TRUE(mode == OperationMode::MANUAL);
    TEST_ASSERT_EQUAL(1, f.http.patchCount);
    // Last POST is the reply relayed to the user
    TEST_ASSERT_TRUE(f.http.lastPostPayload.find("490") != std::string::npos);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_auto_command_switches_mode);
    RUN_TEST(test_manual_command_switches_mode);
    RUN_TEST(test_status_reports_telemetry);
    RUN_TEST(test_status_shows_vr_temp_when_reported);
    RUN_TEST(test_status_omits_vr_temp_when_unsupported);
    RUN_TEST(test_status_shows_pool_info_when_reported);
    RUN_TEST(test_status_omits_pool_info_when_not_configured);
    RUN_TEST(test_status_shows_auto_fan_with_rpm_when_percent_is_zero);
    RUN_TEST(test_esp_reports_system_info);
    RUN_TEST(test_esp_reports_preloaded_reboot_stats);
    RUN_TEST(test_set_applies_valid_settings_and_forces_manual);
    RUN_TEST(test_set_rejects_out_of_range_settings);
    RUN_TEST(test_set_rejects_malformed_input);
    RUN_TEST(test_fan_manual_speed_within_range);
    RUN_TEST(test_fan_rejects_dangerously_low_speed);
    RUN_TEST(test_fan_auto_restores_autofanspeed);
    RUN_TEST(test_restart_posts_to_miner);
    RUN_TEST(test_history_empty_message);
    RUN_TEST(test_history_lists_interventions_after_set);
    RUN_TEST(test_bench_start_forces_manual_and_applies_first_preset);
    RUN_TEST(test_bench_status_and_stop);
    RUN_TEST(test_free_text_goes_to_ai_and_mode_change_is_applied);
    return UNITY_END();
}
