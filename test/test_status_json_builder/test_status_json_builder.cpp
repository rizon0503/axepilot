#include <unity.h>
#include "core/StatusJsonBuilder.h"
#include <ArduinoJson.h>

void setUp(void) {}
void tearDown(void) {}

static BitaxeData sampleData() {
    BitaxeData d;
    d.temperature = 60.5f;
    d.hashrate = 950.25f;
    d.coreVoltage = 1150;
    d.frequency = 550;
    d.power = 15.2f;
    d.fanRpm = 4200;
    d.fanSpeedPercent = 0;
    d.isOverheating = false;
    return d;
}

void test_build_includes_telemetry_fields() {
    float temps[1] = {0.0f};
    std::string json = StatusJsonBuilder::build(sampleData(), OperationMode::AUTOPILOT, true, temps, 0, temps, 0);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(err);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.5f, doc["temperature"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 950.25f, doc["hashrate"].as<float>());
    TEST_ASSERT_EQUAL(1150, doc["coreVoltage"].as<int>());
    TEST_ASSERT_EQUAL(550, doc["frequency"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.2f, doc["power"].as<float>());
    TEST_ASSERT_EQUAL(4200, doc["fanRpm"].as<int>());
    TEST_ASSERT_EQUAL_STRING("AUTO", doc["mode"].as<const char*>());
    TEST_ASSERT_TRUE(doc["wifiOk"].as<bool>());
    TEST_ASSERT_FALSE(doc["isOverheating"].as<bool>());
}

void test_build_reports_manual_mode() {
    float temps[1] = {0.0f};
    std::string json = StatusJsonBuilder::build(sampleData(), OperationMode::MANUAL, true, temps, 0, temps, 0);

    JsonDocument doc;
    deserializeJson(doc, json);
    TEST_ASSERT_EQUAL_STRING("MANUAL", doc["mode"].as<const char*>());
}

void test_build_reports_wifi_down() {
    float temps[1] = {0.0f};
    std::string json = StatusJsonBuilder::build(sampleData(), OperationMode::AUTOPILOT, false, temps, 0, temps, 0);

    JsonDocument doc;
    deserializeJson(doc, json);
    TEST_ASSERT_FALSE(doc["wifiOk"].as<bool>());
}

void test_build_includes_sparkline_arrays() {
    float temps[] = {58.0f, 60.0f, 59.0f};
    float hashes[] = {900.0f, 920.0f, 910.0f};
    std::string json = StatusJsonBuilder::build(sampleData(), OperationMode::AUTOPILOT, true, temps, 3, hashes, 3);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(err);

    JsonArray tempArr = doc["tempHistory"].as<JsonArray>();
    JsonArray hashArr = doc["hashHistory"].as<JsonArray>();
    TEST_ASSERT_EQUAL(3, tempArr.size());
    TEST_ASSERT_EQUAL(3, hashArr.size());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 58.0f, tempArr[0].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 59.0f, tempArr[2].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 910.0f, hashArr[2].as<float>());
}

void test_build_handles_empty_history() {
    float temps[1] = {0.0f};
    std::string json = StatusJsonBuilder::build(sampleData(), OperationMode::AUTOPILOT, true, temps, 0, temps, 0);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL(0, doc["tempHistory"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL(0, doc["hashHistory"].as<JsonArray>().size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_build_includes_telemetry_fields);
    RUN_TEST(test_build_reports_manual_mode);
    RUN_TEST(test_build_reports_wifi_down);
    RUN_TEST(test_build_includes_sparkline_arrays);
    RUN_TEST(test_build_handles_empty_history);
    return UNITY_END();
}
