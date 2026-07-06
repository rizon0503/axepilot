#include "core/TelegramNotifier.h"
#include "core/Limits.h"
#include <ArduinoJson.h>
#include <cstdio>

TelegramNotifier::TelegramNotifier(IHttpClient& httpClient, const std::string& botToken, const std::string& chatId)
    : httpClient(httpClient), botToken(botToken), chatId(chatId), alertSent(false), lastPollTime(0), lastUpdateId(0) {}

void TelegramNotifier::sendMessage(const std::string& text) {
    std::string url = "https://api.telegram.org/bot" + botToken + "/sendMessage";
    JsonDocument doc;
    doc["chat_id"] = chatId;
    doc["text"] = text;
    
    std::string payload;
    serializeJson(doc, payload);
    httpClient.post(url, payload, "");
}

std::string TelegramNotifier::pollNewMessage(uint32_t currentMillis) {
    if (currentMillis - lastPollTime >= 2000 || lastPollTime == 0) {
        lastPollTime = currentMillis;
        
        std::string url = "https://api.telegram.org/bot" + botToken + "/getUpdates?timeout=0";
        if (lastUpdateId != 0) {
            url += "&offset=" + std::to_string(lastUpdateId);
        }
        
        HttpResult httpResult = httpClient.get(url);

        if (httpResult.ok && httpResult.body.size() <= Limits::MAX_JSON_RESPONSE_BYTES) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, httpResult.body);
            if (!err && doc["ok"].as<bool>()) {
                JsonArray result = doc["result"].as<JsonArray>();
                for (JsonObject update : result) {
                    int updateId = update["update_id"];
                    lastUpdateId = updateId + 1; // Acknowledge this message

                    if (update["message"].is<JsonObject>() && update["message"]["text"].is<const char*>()) {
                        // Only accept commands from the configured chat — the bot is
                        // discoverable by anyone, but must obey its owner exclusively.
                        long long fromChat = update["message"]["chat"]["id"] | 0LL;
                        if (std::to_string(fromChat) != chatId) {
                            continue;
                        }
                        return update["message"]["text"].as<std::string>(); // Return the text
                    }
                }
            }
        }
    }
    return ""; // No new message
}

bool TelegramNotifier::checkAndAlert(float temperature) {
    if (temperature >= Limits::TEMP_MAX && !alertSent) {
        char msg[96];
        snprintf(msg, sizeof(msg), "⚠️ CRITICAL: Bitaxe overheating! Temperature: %.1f°C", temperature);
        sendMessage(msg);
        alertSent = true;
        return true;
    } else if (temperature < Limits::TEMP_COLD) {
        alertSent = false;
    }
    return false;
}

void TelegramNotifier::setupCommands() {
    std::string url = "https://api.telegram.org/bot" + botToken + "/setMyCommands";
    
    JsonDocument doc;
    JsonArray cmds = doc["commands"].to<JsonArray>();
    
    JsonObject cmd1 = cmds.add<JsonObject>();
    cmd1["command"] = "status";
    cmd1["description"] = "Current status (frequency, voltage, temp, hashrate)";

    JsonObject cmd2 = cmds.add<JsonObject>();
    cmd2["command"] = "auto";
    cmd2["description"] = "Enable the smart autopilot";

    JsonObject cmd3 = cmds.add<JsonObject>();
    cmd3["command"] = "manual";
    cmd3["description"] = "Disable the autopilot (manual mode)";

    JsonObject cmd4 = cmds.add<JsonObject>();
    cmd4["command"] = "help";
    cmd4["description"] = "Help and instructions";

    JsonObject cmd5 = cmds.add<JsonObject>();
    cmd5["command"] = "esp";
    cmd5["description"] = "ESP32 controller diagnostics";

    JsonObject cmd6 = cmds.add<JsonObject>();
    cmd6["command"] = "history";
    cmd6["description"] = "Journal of recent settings changes";

    JsonObject cmd7 = cmds.add<JsonObject>();
    cmd7["command"] = "bench";
    cmd7["description"] = "Benchmark freq/volt presets (~30 min)";

    JsonObject cmd8 = cmds.add<JsonObject>();
    cmd8["command"] = "why";
    cmd8["description"] = "AI explains the current state";

    std::string payload;
    serializeJson(doc, payload);
    
    httpClient.post(url, payload, "");
}
