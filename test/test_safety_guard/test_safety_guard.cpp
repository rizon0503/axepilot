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

void test_guard_throttles_at_vr_panic_temp() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    BitaxeData data = dataAt(60.0f); // chip temp is fine
    data.vrTemp = Limits::VR_TEMP_PANIC + 1.0f;

    std::string alert = guard.check(data);

    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"frequency\":400") != std::string::npos);
    TEST_ASSERT_TRUE(alert.find("VR") != std::string::npos);
}

void test_guard_idle_below_vr_panic_temp() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    BitaxeData data = dataAt(60.0f);
    data.vrTemp = Limits::VR_TEMP_PANIC - 1.0f;

    std::string alert = guard.check(data);

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(0, alert.length());
}

void test_guard_ignores_invalid_vr_reading() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    BitaxeData data = dataAt(60.0f);
    data.vrTemp = 0.0f; // unsupported firmware / sensor glitch

    std::string alert = guard.check(data);

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(0, alert.length());
}

void test_guard_stays_latched_while_vr_still_hot_after_chip_cools() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    BitaxeData hot = dataAt(60.0f);
    hot.vrTemp = Limits::VR_TEMP_PANIC + 1.0f;
    guard.check(hot); // VR panic fires
    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);

    // Chip was never hot, but VR is still above panic -> stays latched
    BitaxeData stillHotVr = dataAt(60.0f);
    stillHotVr.vrTemp = Limits::VR_TEMP_PANIC + 0.5f;
    guard.check(stillHotVr);
    TEST_ASSERT_EQUAL(1, mockHttp.patchCount); // no duplicate PATCH while latched

    // VR cools below panic -> re-arms
    BitaxeData cooled = dataAt(60.0f);
    cooled.vrTemp = Limits::VR_TEMP_PANIC - 5.0f;
    guard.check(cooled);

    // Next VR panic fires again
    BitaxeData hotAgain = dataAt(60.0f);
    hotAgain.vrTemp = Limits::VR_TEMP_PANIC + 1.0f;
    std::string alert = guard.check(hotAgain);
    TEST_ASSERT_EQUAL(2, mockHttp.patchCount);
    TEST_ASSERT_TRUE(alert.length() > 0);
}

void test_guard_freezes_latch_on_chip_glitch_instead_of_rearming() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    SafetyGuard guard(miner);

    // Chip has reported validly before, so it's known to be a real sensor
    guard.check(dataAt(60.0f));
    // ...then panics
    guard.check(dataAt(Limits::TEMP_PANIC + 1.0f));
    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);

    // A single glitchy 0.0 reading must NOT be mistaken for "cooled down" —
    // freeze the latch instead of guessing (vrTemp was never reported in
    // this test, so it alone would otherwise satisfy the re-arm condition)
    guard.check(dataAt(0.0f));
    TEST_ASSERT_EQUAL(1, mockHttp.patchCount); // still latched, no re-arm

    // A genuinely cool reading re-arms normally
    guard.check(dataAt(Limits::TEMP_MAX - 2.0f));
    std::string alert = guard.check(dataAt(Limits::TEMP_PANIC + 1.0f));
    TEST_ASSERT_EQUAL(2, mockHttp.patchCount);
    TEST_ASSERT_TRUE(alert.length() > 0);
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
    RUN_TEST(test_guard_throttles_at_vr_panic_temp);
    RUN_TEST(test_guard_idle_below_vr_panic_temp);
    RUN_TEST(test_guard_ignores_invalid_vr_reading);
    RUN_TEST(test_guard_stays_latched_while_vr_still_hot_after_chip_cools);
    RUN_TEST(test_guard_freezes_latch_on_chip_glitch_instead_of_rearming);
    RUN_TEST(test_guard_latches_and_rearms_after_cooldown);
    return UNITY_END();
}
