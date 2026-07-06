#include <unity.h>
#include "core/TelegramNotifier.h"
#include "core/Limits.h"
#include "../mocks/MockHttpClient.h"

void setUp(void) {}
void tearDown(void) {}

void test_thermal_alert_triggered_at_max_temp() {
    MockHttpClient mockHttp;
    TelegramNotifier notifier(mockHttp, "dummy_token", "dummy_chat");

    bool alerted = notifier.checkAndAlert(Limits::TEMP_MAX + 0.5f);

    TEST_ASSERT_TRUE(alerted);
    TEST_ASSERT_EQUAL(1, mockHttp.postCount);
}

void test_thermal_alert_not_triggered_below_max_temp() {
    MockHttpClient mockHttp;
    TelegramNotifier notifier(mockHttp, "dummy_token", "dummy_chat");

    bool alerted = notifier.checkAndAlert(Limits::TEMP_MAX - 1.0f);

    TEST_ASSERT_FALSE(alerted);
    TEST_ASSERT_EQUAL(0, mockHttp.postCount);
}

void test_thermal_alert_latches_until_cooled_down() {
    MockHttpClient mockHttp;
    TelegramNotifier notifier(mockHttp, "dummy_token", "dummy_chat");

    TEST_ASSERT_TRUE(notifier.checkAndAlert(Limits::TEMP_MAX + 1.0f));
    // Still hot: no duplicate alert spam
    TEST_ASSERT_FALSE(notifier.checkAndAlert(Limits::TEMP_MAX + 2.0f));
    TEST_ASSERT_EQUAL(1, mockHttp.postCount);

    // Cooling below TEMP_COLD resets the latch
    notifier.checkAndAlert(Limits::TEMP_COLD - 1.0f);
    TEST_ASSERT_TRUE(notifier.checkAndAlert(Limits::TEMP_MAX + 1.0f));
    TEST_ASSERT_EQUAL(2, mockHttp.postCount);
}

void test_poll_ignores_messages_from_foreign_chats() {
    MockHttpClient mockHttp;
    TelegramNotifier notifier(mockHttp, "dummy_token", "12345");

    mockHttp.getResponse =
        "{\"ok\":true,\"result\":["
        "{\"update_id\":1,\"message\":{\"chat\":{\"id\":999},\"text\":\"/hack\"}},"
        "{\"update_id\":2,\"message\":{\"chat\":{\"id\":12345},\"text\":\"/status\"}}"
        "]}";

    std::string msg = notifier.pollNewMessage(5000);

    TEST_ASSERT_EQUAL_STRING("/status", msg.c_str());
}

void test_poll_returns_empty_when_only_foreign_chats() {
    MockHttpClient mockHttp;
    TelegramNotifier notifier(mockHttp, "dummy_token", "12345");

    mockHttp.getResponse =
        "{\"ok\":true,\"result\":["
        "{\"update_id\":1,\"message\":{\"chat\":{\"id\":999},\"text\":\"/set 625 1250\"}}"
        "]}";

    std::string msg = notifier.pollNewMessage(5000);

    TEST_ASSERT_EQUAL(0, msg.length());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_thermal_alert_triggered_at_max_temp);
    RUN_TEST(test_thermal_alert_not_triggered_below_max_temp);
    RUN_TEST(test_thermal_alert_latches_until_cooled_down);
    RUN_TEST(test_poll_ignores_messages_from_foreign_chats);
    RUN_TEST(test_poll_returns_empty_when_only_foreign_chats);
    return UNITY_END();
}
