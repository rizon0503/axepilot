#include <unity.h>
#include <ArduinoJson.h>
#include "core/DeepSeekOptimizer.h"
#include "core/BitaxeController.h"
#include "core/Limits.h"
#include "../mocks/MockHttpClient.h"
#include "../mocks/MockSystemTime.h"

void setUp(void) {}
void tearDown(void) {}

// Shared empty history for tests that don't care about trends
static TelemetryHistory g_emptyHistory;

static std::string deepseekReply(const std::string& innerJson) {
    // DeepSeek returns the model output as an escaped JSON string in content
    std::string escaped;
    for (char c : innerJson) {
        if (c == '"') escaped += "\\\"";
        else escaped += c;
    }
    return "{\"choices\":[{\"message\":{\"content\":\"" + escaped + "\"}}]}";
}

static BitaxeData overheatingData() {
    BitaxeData data{};
    data.temperature = 72.0f;
    data.hashrate = 1200.0f;
    data.coreVoltage = 1250;
    data.frequency = 625;
    data.isOverheating = true;
    data.isTooCold = false;
    return data;
}

void test_optimizer_applies_valid_ai_settings() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"frequency\":490,\"coreVoltage\":1100,\"reason\":\"ok\"}");
    mockTime.currentTime = 60000;

    std::string report = optimizer.optimize(overheatingData(), g_emptyHistory);

    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"frequency\":490") != std::string::npos);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"coreVoltage\":1100") != std::string::npos);
    TEST_ASSERT_TRUE(report.length() > 0);
}

void test_optimizer_rejects_out_of_range_ai_settings() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    // AI hallucinates dangerous values above the allowed maximum
    mockHttp.postResponse = deepseekReply("{\"frequency\":800,\"coreVoltage\":1400,\"reason\":\"turbo\"}");
    mockTime.currentTime = 60000;

    std::string report = optimizer.optimize(overheatingData(), g_emptyHistory);

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(0, report.length());
}

void test_optimizer_respects_cooldown() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"frequency\":490,\"coreVoltage\":1100,\"reason\":\"ok\"}");
    mockTime.currentTime = 60000;
    optimizer.optimize(overheatingData(), g_emptyHistory);
    TEST_ASSERT_EQUAL(1, mockHttp.postCount);

    // 10 s later: still inside the 30 s cooldown, no new API call
    mockTime.currentTime = 70000;
    optimizer.optimize(overheatingData(), g_emptyHistory);
    TEST_ASSERT_EQUAL(1, mockHttp.postCount);

    // 31 s later: cooldown expired
    mockTime.currentTime = 91000;
    optimizer.optimize(overheatingData(), g_emptyHistory);
    TEST_ASSERT_EQUAL(2, mockHttp.postCount);
}

void test_optimizer_idle_when_temperature_is_normal() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    BitaxeData data{};
    data.temperature = 65.0f; // Between TEMP_COLD and TEMP_MAX
    data.frequency = 550;
    mockTime.currentTime = 60000;

    std::string report = optimizer.optimize(data, g_emptyHistory);

    TEST_ASSERT_EQUAL(0, mockHttp.postCount);
    TEST_ASSERT_EQUAL(0, report.length());
}

void test_optimizer_includes_temperature_trend_in_prompt() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    TelemetryHistory history;
    BitaxeData s1{}; s1.temperature = 68.0f; s1.hashrate = 1200.0f;
    BitaxeData s2{}; s2.temperature = 72.0f; s2.hashrate = 1180.0f;
    history.record(s1, 0);
    history.record(s2, 120000);

    mockHttp.postResponse = deepseekReply("{\"frequency\":490,\"coreVoltage\":1100,\"reason\":\"ok\"}");
    mockTime.currentTime = 200000;

    optimizer.optimize(overheatingData(), history);

    TEST_ASSERT_TRUE(mockHttp.lastPostPayload.find("History: temp trend") != std::string::npos);
}

void test_optimizer_falls_back_locally_after_three_ai_failures() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    // The AI endpoint is down: responses are garbage
    mockHttp.postResponse = "<html>502 Bad Gateway</html>";

    mockTime.currentTime = 60000;
    TEST_ASSERT_EQUAL(0, optimizer.optimize(overheatingData(), g_emptyHistory).length()); // failure 1
    mockTime.currentTime = 91000;
    TEST_ASSERT_EQUAL(0, optimizer.optimize(overheatingData(), g_emptyHistory).length()); // failure 2
    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);

    mockTime.currentTime = 122000;
    std::string report = optimizer.optimize(overheatingData(), g_emptyHistory); // failure 3 -> local fallback

    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"frequency\":400") != std::string::npos);
    TEST_ASSERT_TRUE(mockHttp.lastPatchPayload.find("\"coreVoltage\":1000") != std::string::npos);
    TEST_ASSERT_TRUE(report.length() > 0);
}

void test_optimizer_rejects_oversized_response_without_parsing() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    // Otherwise-valid JSON that would apply settings if parsed
    std::string padding(Limits::MAX_JSON_RESPONSE_BYTES + 1, 'x');
    mockHttp.postResponse = deepseekReply("{\"frequency\":490,\"coreVoltage\":1100,\"reason\":\"" + padding + "\"}");
    mockTime.currentTime = 60000;

    std::string report = optimizer.optimize(overheatingData(), g_emptyHistory);

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_EQUAL(0, report.length());
}

void test_ask_question_rejects_oversized_response_without_parsing() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    std::string padding(Limits::MAX_JSON_RESPONSE_BYTES + 1, 'x');
    mockHttp.postResponse = deepseekReply("{\"reply\":\"" + padding + "\",\"frequency\":550,\"coreVoltage\":1150}");

    AiChatResult result = optimizer.askQuestion("set it to 550", overheatingData());

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_FALSE(result.settingsApplied);
}

void test_ask_question_rejects_out_of_range_settings() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Overclocking!\",\"frequency\":900,\"coreVoltage\":1500}");

    AiChatResult result = optimizer.askQuestion("overclock to the max", overheatingData());

    TEST_ASSERT_EQUAL(0, mockHttp.patchCount);
    TEST_ASSERT_FALSE(result.settingsApplied);
    TEST_ASSERT_TRUE(result.reply.find("rejected") != std::string::npos);
}

void test_ask_question_applies_valid_settings() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Done\",\"frequency\":550,\"coreVoltage\":1150}");

    AiChatResult result = optimizer.askQuestion("set it to 550", overheatingData());

    TEST_ASSERT_EQUAL(1, mockHttp.patchCount);
    TEST_ASSERT_TRUE(result.settingsApplied);
}

void test_ask_question_replays_conversation_context() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Current frequency is 550\"}");
    optimizer.askQuestion("what's the frequency?", overheatingData());

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Ok\"}");
    optimizer.askQuestion("now 25 more", overheatingData());

    // The second request must contain the first exchange
    TEST_ASSERT_TRUE(mockHttp.lastPostPayload.find("what's the frequency?") != std::string::npos);
    TEST_ASSERT_TRUE(mockHttp.lastPostPayload.find("Current frequency is 550") != std::string::npos);
    TEST_ASSERT_TRUE(mockHttp.lastPostPayload.find("assistant") != std::string::npos);
}

// Guards against accidentally breaking the DeepSeek prompt: these parse the
// actual outgoing request JSON (not just substring-search it) and assert on
// the structural pieces a working integration depends on.
void test_optimize_request_has_well_formed_structure() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"frequency\":490,\"coreVoltage\":1100,\"reason\":\"ok\"}");
    mockTime.currentTime = 60000;

    optimizer.optimize(overheatingData(), g_emptyHistory);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, mockHttp.lastPostPayload);
    TEST_ASSERT_FALSE(err);

    TEST_ASSERT_EQUAL_STRING("deepseek-chat", doc["model"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("json_object", doc["response_format"]["type"].as<const char*>());

    JsonArray messages = doc["messages"].as<JsonArray>();
    TEST_ASSERT_EQUAL(2, messages.size());
    TEST_ASSERT_EQUAL_STRING("system", messages[0]["role"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("user", messages[1]["role"].as<const char*>());

    std::string userContent = messages[1]["content"].as<const char*>();
    TEST_ASSERT_TRUE(userContent.find("Temp=72.0") != std::string::npos);
    TEST_ASSERT_TRUE(userContent.find("Freq=625") != std::string::npos);
    TEST_ASSERT_TRUE(userContent.find("Volt=1250") != std::string::npos);
}

void test_ask_question_request_has_well_formed_structure() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Done\"}");

    optimizer.askQuestion("what's going on?", overheatingData());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, mockHttp.lastPostPayload);
    TEST_ASSERT_FALSE(err);

    TEST_ASSERT_EQUAL_STRING("deepseek-chat", doc["model"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("json_object", doc["response_format"]["type"].as<const char*>());

    JsonArray messages = doc["messages"].as<JsonArray>();
    TEST_ASSERT_EQUAL(2, messages.size()); // no prior chat history yet
    TEST_ASSERT_EQUAL_STRING("system", messages[0]["role"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("user", messages[1]["role"].as<const char*>());

    std::string userContent = messages[1]["content"].as<const char*>();
    TEST_ASSERT_TRUE(userContent.find("what's going on?") != std::string::npos);
    TEST_ASSERT_TRUE(userContent.find("Temp=72.0") != std::string::npos);
}

void test_ask_question_request_structure_includes_replayed_history() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"reply\":\"550 MHz\"}");
    optimizer.askQuestion("what's the frequency?", overheatingData());

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Ok, done\"}");
    optimizer.askQuestion("now 25 more", overheatingData());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, mockHttp.lastPostPayload);
    TEST_ASSERT_FALSE(err);

    JsonArray messages = doc["messages"].as<JsonArray>();
    // system + (past user, past assistant) + new user = 4
    TEST_ASSERT_EQUAL(4, messages.size());
    TEST_ASSERT_EQUAL_STRING("system", messages[0]["role"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("user", messages[1]["role"].as<const char*>());
    TEST_ASSERT_TRUE(std::string(messages[1]["content"].as<const char*>()).find("what's the frequency?") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("assistant", messages[2]["role"].as<const char*>());
    TEST_ASSERT_TRUE(std::string(messages[2]["content"].as<const char*>()).find("550 MHz") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("user", messages[3]["role"].as<const char*>());
    TEST_ASSERT_TRUE(std::string(messages[3]["content"].as<const char*>()).find("now 25 more") != std::string::npos);
}

void test_optimizer_uses_custom_endpoint_and_model() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner,
                                "https://example.com/v1/chat/completions", "my-model");

    mockHttp.postResponse = deepseekReply("{\"frequency\":490,\"coreVoltage\":1100,\"reason\":\"ok\"}");
    mockTime.currentTime = 60000;

    optimizer.optimize(overheatingData(), g_emptyHistory);

    TEST_ASSERT_EQUAL_STRING("https://example.com/v1/chat/completions", mockHttp.lastPostUrl.c_str());
    TEST_ASSERT_TRUE(mockHttp.lastPostPayload.find("my-model") != std::string::npos);
}

void test_ask_question_reports_mode_change_structurally() {
    MockHttpClient mockHttp;
    MockSystemTime mockTime;
    BitaxeController miner(mockHttp, mockTime, "192.168.0.128");
    DeepSeekOptimizer optimizer(mockHttp, mockTime, "dummy_key", miner);

    mockHttp.postResponse = deepseekReply("{\"reply\":\"Enabling autopilot\",\"mode\":\"auto\"}");

    AiChatResult result = optimizer.askQuestion("turn on auto", overheatingData());

    TEST_ASSERT_TRUE(result.modeChange == AiChatResult::ModeChange::Auto);
    TEST_ASSERT_FALSE(result.settingsApplied);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_optimizer_applies_valid_ai_settings);
    RUN_TEST(test_optimizer_rejects_out_of_range_ai_settings);
    RUN_TEST(test_optimizer_respects_cooldown);
    RUN_TEST(test_optimizer_idle_when_temperature_is_normal);
    RUN_TEST(test_optimizer_includes_temperature_trend_in_prompt);
    RUN_TEST(test_optimizer_falls_back_locally_after_three_ai_failures);
    RUN_TEST(test_optimizer_rejects_oversized_response_without_parsing);
    RUN_TEST(test_ask_question_rejects_oversized_response_without_parsing);
    RUN_TEST(test_ask_question_rejects_out_of_range_settings);
    RUN_TEST(test_ask_question_applies_valid_settings);
    RUN_TEST(test_ask_question_replays_conversation_context);
    RUN_TEST(test_optimize_request_has_well_formed_structure);
    RUN_TEST(test_ask_question_request_has_well_formed_structure);
    RUN_TEST(test_ask_question_request_structure_includes_replayed_history);
    RUN_TEST(test_optimizer_uses_custom_endpoint_and_model);
    RUN_TEST(test_ask_question_reports_mode_change_structurally);
    return UNITY_END();
}
