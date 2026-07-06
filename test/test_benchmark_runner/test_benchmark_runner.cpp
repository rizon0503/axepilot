#include <unity.h>
#include "core/BenchmarkRunner.h"
#include "core/Limits.h"
#include "../mocks/MockHttpClient.h"
#include "../mocks/MockSystemTime.h"
#include <string>

void setUp(void) {}
void tearDown(void) {}

static BitaxeData sample(float temp, float hash, float power) {
    BitaxeData data{};
    data.temperature = temp;
    data.hashrate = hash;
    data.power = power;
    return data;
}

void test_start_applies_first_preset() {
    MockHttpClient http;
    MockSystemTime time;
    BitaxeController miner(http, time, "192.168.0.128");
    BenchmarkRunner bench(miner);

    std::string msg = bench.start(0);

    TEST_ASSERT_TRUE(bench.isRunning());
    TEST_ASSERT_EQUAL(1, http.patchCount);
    TEST_ASSERT_TRUE(http.lastPatchPayload.find("\"frequency\":400") != std::string::npos);
    TEST_ASSERT_TRUE(msg.find("1/6") != std::string::npos);
}

void test_tick_advances_to_next_preset_after_dwell() {
    MockHttpClient http;
    MockSystemTime time;
    BitaxeController miner(http, time, "192.168.0.128");
    BenchmarkRunner bench(miner);

    bench.start(0);
    // Sample after warmup, then cross the dwell boundary
    TEST_ASSERT_EQUAL(0, bench.tick(sample(55.0f, 800.0f, 12.0f), BenchmarkRunner::WARMUP_MS + 1000).length());
    std::string msg = bench.tick(sample(55.0f, 800.0f, 12.0f), BenchmarkRunner::DWELL_MS + 1000);

    TEST_ASSERT_TRUE(msg.find("2/6") != std::string::npos);
    TEST_ASSERT_EQUAL(2, http.patchCount);
    TEST_ASSERT_TRUE(http.lastPatchPayload.find("\"frequency\":490") != std::string::npos);
}

void test_tick_does_not_sample_during_warmup() {
    MockHttpClient http;
    MockSystemTime time;
    BitaxeController miner(http, time, "192.168.0.128");
    BenchmarkRunner bench(miner);

    bench.start(0);
    bench.tick(sample(55.0f, 800.0f, 12.0f), BenchmarkRunner::WARMUP_MS - 5000);

    std::string prog = bench.progress(BenchmarkRunner::WARMUP_MS - 4000);
    TEST_ASSERT_TRUE(prog.find("Samples: 0") != std::string::npos);
}

void test_benchmark_aborts_on_overheat() {
    MockHttpClient http;
    MockSystemTime time;
    BitaxeController miner(http, time, "192.168.0.128");
    BenchmarkRunner bench(miner);

    bench.start(0);
    std::string msg = bench.tick(sample(Limits::TEMP_MAX + 0.5f, 1200.0f, 18.0f), 60000);

    TEST_ASSERT_FALSE(bench.isRunning());
    TEST_ASSERT_TRUE(msg.find("aborted") != std::string::npos);
    // Safe settings applied after the abort
    TEST_ASSERT_TRUE(http.lastPatchPayload.find("\"frequency\":400") != std::string::npos);
    TEST_ASSERT_EQUAL(2, http.patchCount);
}

void test_full_run_reports_best_efficiency_and_applies_it() {
    MockHttpClient http;
    MockSystemTime time;
    BitaxeController miner(http, time, "192.168.0.128");
    BenchmarkRunner bench(miner);

    bench.start(0);

    // Step data: preset 4 (550/1150) gets the best GH/W (100), others 50-60
    float hashes[6] = {600.0f, 700.0f, 800.0f, 1500.0f, 900.0f, 950.0f};
    float powers[6] = {12.0f, 13.0f, 14.0f, 15.0f, 17.0f, 19.0f};

    std::string finalMsg;
    for (int step = 0; step < 6; step++) {
        uint32_t base = (uint32_t)step * BenchmarkRunner::DWELL_MS;
        // one measurement inside the measuring window
        bench.tick(sample(60.0f, hashes[step], powers[step]), base + BenchmarkRunner::WARMUP_MS + 1000);
        // cross the dwell boundary
        finalMsg = bench.tick(sample(60.0f, hashes[step], powers[step]), base + BenchmarkRunner::DWELL_MS + 1000);
    }

    TEST_ASSERT_FALSE(bench.isRunning());
    TEST_ASSERT_TRUE(finalMsg.find("finished") != std::string::npos);
    TEST_ASSERT_TRUE(finalMsg.find("Optimum: 550 MHz / 1150 mV") != std::string::npos);
    // Best preset re-applied at the end
    TEST_ASSERT_TRUE(http.lastPatchPayload.find("\"frequency\":550") != std::string::npos);
}

void test_manual_stop_restores_safe_settings() {
    MockHttpClient http;
    MockSystemTime time;
    BitaxeController miner(http, time, "192.168.0.128");
    BenchmarkRunner bench(miner);

    bench.start(0);
    std::string msg = bench.stop();

    TEST_ASSERT_FALSE(bench.isRunning());
    TEST_ASSERT_TRUE(msg.find("stopped") != std::string::npos);
    TEST_ASSERT_TRUE(http.lastPatchPayload.find("\"frequency\":400") != std::string::npos);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_start_applies_first_preset);
    RUN_TEST(test_tick_advances_to_next_preset_after_dwell);
    RUN_TEST(test_tick_does_not_sample_during_warmup);
    RUN_TEST(test_benchmark_aborts_on_overheat);
    RUN_TEST(test_full_run_reports_best_efficiency_and_applies_it);
    RUN_TEST(test_manual_stop_restores_safe_settings);
    return UNITY_END();
}
