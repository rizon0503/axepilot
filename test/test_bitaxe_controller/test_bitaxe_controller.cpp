#include <unity.h>
#include "core/BitaxeController.h"
#include "core/Limits.h"
#include "../mocks/MockHttpClient.h"
#include "../mocks/MockSystemTime.h"

void setUp(void) {}
void tearDown(void) {}

void test_controller_fetches_and_parses_data() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController controller(mockHttp, mockTime, "192.168.0.128");

    mockHttp.getResponse = "{\"temp\": 60.5, \"hashRate\": 500, \"coreVoltage\": 1200, \"frequency\": 485}";
    mockTime.currentTime = 5000; // Trigger an update
    controller.update();

    BitaxeData data = controller.getData();
    TEST_ASSERT_EQUAL_FLOAT(60.5f, data.temperature);
    TEST_ASSERT_EQUAL_FLOAT(500.0f, data.hashrate);
    TEST_ASSERT_EQUAL(1200, data.coreVoltage);
    TEST_ASSERT_EQUAL(485, data.frequency);
    TEST_ASSERT_FALSE(data.isOverheating);
    TEST_ASSERT_FALSE(data.isTooCold); // 60.5 is just above the TEMP_COLD threshold
}

void test_controller_flags_overheating() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController controller(mockHttp, mockTime, "192.168.0.128");

    mockHttp.getResponse = "{\"temp\": 71.0, \"hashRate\": 1200, \"coreVoltage\": 1250, \"frequency\": 625}";
    mockTime.currentTime = 5000;
    controller.update();

    BitaxeData data = controller.getData();
    TEST_ASSERT_TRUE(data.isOverheating);
    TEST_ASSERT_FALSE(data.isTooCold);
}

void test_controller_treats_zero_temp_as_invalid_reading() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController controller(mockHttp, mockTime, "192.168.0.128");

    mockHttp.getResponse = "{\"temp\": 0.0, \"hashRate\": 1200, \"coreVoltage\": 1150, \"frequency\": 550}";
    mockTime.currentTime = 5000;
    controller.update();

    BitaxeData data = controller.getData();
    TEST_ASSERT_FALSE(data.isOverheating);
    TEST_ASSERT_FALSE(data.isTooCold); // 0.0C is a sensor glitch, not "too cold"
}

void test_controller_parses_extended_telemetry() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController controller(mockHttp, mockTime, "192.168.0.128");

    mockHttp.getResponse =
        "{\"temp\": 63.0, \"hashRate\": 1100, \"coreVoltage\": 1150, \"frequency\": 550,"
        " \"power\": 15.2, \"fanspeed\": 82, \"fanrpm\": 4200,"
        " \"sharesAccepted\": 5432, \"sharesRejected\": 7,"
        " \"bestDiff\": \"43.9M\", \"uptimeSeconds\": 86400}";
    mockTime.currentTime = 5000;
    controller.update();

    BitaxeData data = controller.getData();
    TEST_ASSERT_EQUAL_FLOAT(15.2f, data.power);
    TEST_ASSERT_EQUAL(82, data.fanSpeedPercent);
    TEST_ASSERT_EQUAL(4200, data.fanRpm);
    TEST_ASSERT_EQUAL(5432, data.sharesAccepted);
    TEST_ASSERT_EQUAL(7, data.sharesRejected);
    TEST_ASSERT_EQUAL_STRING("43.9M", data.bestDiff);
    TEST_ASSERT_EQUAL(86400, data.uptimeSeconds);
}

void test_controller_ignores_oversized_response() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController controller(mockHttp, mockTime, "192.168.0.128");

    // Otherwise-valid JSON, but padded well past Limits::MAX_JSON_RESPONSE_BYTES —
    // must be rejected on size alone, before ArduinoJson ever sees it.
    std::string padding(Limits::MAX_JSON_RESPONSE_BYTES + 1, 'x');
    mockHttp.getResponse = "{\"temp\": 99.9, \"padding\": \"" + padding + "\"}";
    mockTime.currentTime = 5000;
    controller.update();

    BitaxeData data = controller.getData();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, data.temperature); // untouched default, update was skipped
}

void test_controller_rate_limits_polling() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController controller(mockHttp, mockTime, "192.168.0.128");

    mockHttp.getResponse = "{\"temp\": 55.0, \"hashRate\": 1000, \"coreVoltage\": 1100, \"frequency\": 500}";
    mockTime.currentTime = 5000;
    controller.update();
    mockTime.currentTime = 6000; // Only 1 s later — inside the 2 s window
    controller.update();

    TEST_ASSERT_EQUAL(1, mockHttp.getCount);

    mockTime.currentTime = 7500; // 2.5 s after the first poll
    controller.update();
    TEST_ASSERT_EQUAL(2, mockHttp.getCount);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_controller_fetches_and_parses_data);
    RUN_TEST(test_controller_flags_overheating);
    RUN_TEST(test_controller_treats_zero_temp_as_invalid_reading);
    RUN_TEST(test_controller_parses_extended_telemetry);
    RUN_TEST(test_controller_ignores_oversized_response);
    RUN_TEST(test_controller_rate_limits_polling);
    return UNITY_END();
}
