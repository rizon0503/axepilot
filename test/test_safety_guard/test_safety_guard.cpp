#include <unity.h>
#include "core/SafetyGuard.h"
#include "core/Limits.h"
#include "../mocks/MockHttpClient.h"
#include "../mocks/MockSystemTime.h"

void setUp(void) {}
void tearDown(void) {}

static BitaxeData dataAt(float temp) {
    BitaxeData data{};
    data.temperature = temp;
    data.frequency = 600;
    data.coreVoltage = 1200;
    data.isOverheating = temp >= Limits::TEMP_MAX;
    return data;
}

void test_guard_throttles_at_panic_temp() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    std::string alert = guard.check(dataAt(Limits::TEMP_PANIC + 0.5f));

    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"frequency\":400") != std::string::npos);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"coreVoltage\":1000") != std::string::npos);
    TEST_ASSERT_TRUE(alert.length() > 0);
}

void test_guard_idle_below_panic_temp() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    std::string alert = guard.check(dataAt(Limits::TEMP_PANIC - 0.5f));

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(0, alert.length());
}

void test_guard_ignores_invalid_zero_reading() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    std::string alert = guard.check(dataAt(0.0f));

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(0, alert.length());
}

void test_guard_latches_and_rearms_after_cooldown() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    // First incident: throttle once, then stay quiet while still hot
    guard.check(dataAt(Limits::TEMP_PANIC + 1.0f));
    guard.check(dataAt(Limits::TEMP_PANIC + 0.2f));
    guard.check(dataAt(Limits::TEMP_MAX + 0.5f)); // below panic but still above max
    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);

    // Cooled below TEMP_MAX: guard re-arms
    guard.check(dataAt(Limits::TEMP_MAX - 2.0f));

    // Second incident fires again
    std::string alert = guard.check(dataAt(Limits::TEMP_PANIC + 1.0f));
    TEST_ASSERT_EQUAL(2, mockHttp.patchCount);
    TEST_ASSERT_TRUE(alert.length() > 0);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_guard_throttles_at_panic_temp);
    RUN_TEST(test_guard_idle_below_panic_temp);
    RUN_TEST(test_guard_ignores_invalid_zero_reading);
    RUN_TEST(test_guard_latches_and_rearms_after_cooldown);
    return UNITY_END();
}
