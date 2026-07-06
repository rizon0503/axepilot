#include "core/DeepSeekOptimizer.h"
#include "core/Limits.h"
#include <ArduinoJson.h>
#include <cstdio>

DeepSeekOptimizer::DeepSeekOptimizer(IHttpClient& httpClient, ISystemTime& sysTime, const std::string& apiKey, BitaxeController& miner,
                                     const std::string& baseUrl, const std::string& model)
    : httpClient(httpClient), sysTime(sysTime), apiKey(apiKey), miner(miner),
      baseUrl(baseUrl), model(model), lastOptimizeTime(0), aiFailureCount(0) {}

std::string DeepSeekOptimizer::optimize(const BitaxeData& data, const TelemetryHistory& history) {
    uint32_t now = sysTime.millis();

    // 30 seconds cooldown to prevent API spamming
    if (now - lastOptimizeTime < 30000 && lastOptimizeTime != 0) {
        return "";
    }

    if (data.isOverheating || (data.isTooCold && data.frequency < 550)) {
        lastOptimizeTime = now;

        JsonDocument doc;
        doc["model"] = model;
        doc["response_format"]["type"] = "json_object"; // Force JSON format from the model

        JsonArray messages = doc["messages"].to<JsonArray>();
        JsonObject sysMsg = messages.add<JsonObject>();
        sysMsg["role"] = "system";

        if (data.isOverheating) {
            sysMsg["content"] = "You are an AI for Bitaxe Gamma. Strict max temp is 70C. Output ONLY a valid JSON object with keys 'frequency' (int), 'coreVoltage' (int), and 'reason' (string in English explaining the logic). Provide safer (lower) settings. Allowed Freqs: 400,490,525,550,600,625. Allowed Volts: 1000,1060,1100,1150,1200,1250.";
        } else {
            sysMsg["content"] = "You are an AI for Bitaxe Gamma. The device is too cold and underperforming. Output ONLY a valid JSON object with keys 'frequency' (int), 'coreVoltage' (int), and 'reason' (string in English explaining the logic). Provide slightly HIGHER settings to maximize hashrate up to 550MHz. Allowed Freqs: 400,490,525,550,600,625. Allowed Volts: 1000,1060,1100,1150,1200,1250.";
        }

        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";

        char telemetry[352];
        int written = snprintf(telemetry, sizeof(telemetry), "Current: Temp=%.1f, Freq=%d, Volt=%d, Power=%.1fW. %s",
                 data.temperature, data.frequency, data.coreVoltage, data.power,
                 data.isOverheating ? "It is overheating! Lower the settings." : "It is too cold. Increase settings by one step.");
        // Let the AI see the direction of change, not just one snapshot
        if (written > 0 && (size_t)written < sizeof(telemetry)) {
            history.summarize(telemetry + written, sizeof(telemetry) - written);
        }
        userMsg["content"] = telemetry;

        std::string payload;
        serializeJson(doc, payload);

        std::string headers = "Bearer " + apiKey;
        std::string response = httpClient.post(baseUrl, payload, headers);

        if (!response.empty()) {
            JsonDocument respDoc;
            if (!deserializeJson(respDoc, response)) {
                std::string content = respDoc["choices"][0]["message"]["content"] | "{}";
                JsonDocument contentDoc;
                if (!deserializeJson(contentDoc, content)) {
                    int newFreq = contentDoc["frequency"] | 0;
                    int newVolt = contentDoc["coreVoltage"] | 0;
                    std::string reason = contentDoc["reason"] | "Optimization for better stability.";

                    // Apply safety checks before changing parameters
                    if (Limits::isValidSetting(newFreq, newVolt)) {
                        aiFailureCount = 0;
                        miner.applySettings(newFreq, newVolt, "Autopilot");

                        char msg[512];
                        snprintf(msg, sizeof(msg), "🤖 **Autopilot intervened!**\n\n🌡️ Temperature was: %.1f°C\n⚙️ New settings: %d MHz / %d mV\n\n💭 Reason: %s",
                                 data.temperature, newFreq, newVolt, reason.c_str());
                        return std::string(msg);
                    }
                }
            }
        }

        // The AI call failed (network error, garbage response or unsafe values).
        // While overheating we cannot afford to wait forever: after 3 failed
        // attempts (~90 s with the 30 s cooldown) fall back to safe settings
        // locally, without the cloud.
        if (data.isOverheating) {
            aiFailureCount++;
            if (aiFailureCount >= 3) {
                aiFailureCount = 0;
                miner.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "AI fallback");

                char msg[224];
                snprintf(msg, sizeof(msg),
                         "⚠️ AI unavailable (3 failed attempts) and Bitaxe is overheating (%.1f°C).\nLocal fallback applied %d MHz / %d mV.",
                         data.temperature, Limits::FREQ_MIN, Limits::VOLT_MIN);
                return std::string(msg);
            }
        }
    }
    return "";
}

AiChatResult DeepSeekOptimizer::askQuestion(const std::string& question, const BitaxeData& data) {
    AiChatResult result;

    JsonDocument doc;
    doc["model"] = model;
    doc["max_tokens"] = 200; // Prevent OOM and timeouts for long answers

    doc["response_format"]["type"] = "json_object"; // Force JSON

    JsonArray messages = doc["messages"].to<JsonArray>();
    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "You are an AI assistant for Bitaxe. You can change settings or operation mode if asked. Output strictly JSON: {\"reply\": \"Your message in English\", \"frequency\": 400, \"coreVoltage\": 1000, \"mode\": \"auto\" (or \"manual\")}. Omit freq/volt/mode if no change is needed. Allowed Freqs: 400,490,525,550,600,625. Allowed Volts: 1000,1060,1100,1150,1200,1250.";

    // Replay recent exchanges so follow-up questions keep their context
    for (size_t i = 0; i < chatMemory.size(); i++) {
        const ChatMemory::Exchange& exchange = chatMemory.get(i);
        JsonObject pastUser = messages.add<JsonObject>();
        pastUser["role"] = "user";
        pastUser["content"] = exchange.user;
        JsonObject pastAssistant = messages.add<JsonObject>();
        pastAssistant["role"] = "assistant";
        pastAssistant["content"] = exchange.assistant;
    }

    JsonObject userMsg = messages.add<JsonObject>();
    userMsg["role"] = "user";

    char telemetry[128];
    snprintf(telemetry, sizeof(telemetry), "[Telemetry: Temp=%.1fC, Hashrate=%.1fGH/s, Volt=%dmV, Freq=%dMHz] User: ", data.temperature, data.hashrate, data.coreVoltage, data.frequency);

    userMsg["content"] = std::string(telemetry) + question;

    std::string payload;
    serializeJson(doc, payload);

    std::string headers = "Bearer " + apiKey;
    std::string response = httpClient.post(baseUrl, payload, headers);

    if (response.empty()) {
        result.reply = "Failed: Response totally empty.";
        return result;
    }

    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, response);

    if (err) {
        result.reply = "JSON Parse Error: " + std::string(err.c_str()) + " Code: " + response.substr(0, 20);
        return result;
    }

    if (respDoc["error_http"].is<int>()) {
        result.reply = "HTTP Error: " + respDoc["error_http"].as<std::string>() + " " + respDoc["error_msg"].as<std::string>();
        return result;
    }

    if (respDoc["error"].is<JsonObject>()) {
        result.reply = "API Error: " + respDoc["error"]["message"].as<std::string>();
        return result;
    }

    std::string content = respDoc["choices"][0]["message"]["content"] | "{}";

    JsonDocument contentDoc;
    DeserializationError parseErr = deserializeJson(contentDoc, content);
    if (parseErr) {
        result.reply = "⚠️ AI Format Error: " + std::string(parseErr.c_str()) + "\nRaw AI response: " + content;
        return result;
    }

    result.reply = contentDoc["reply"] | "Done.";

    std::string modeStr = contentDoc["mode"] | "";
    if (modeStr == "auto" || modeStr == "AUTOPILOT") {
        result.modeChange = AiChatResult::ModeChange::Auto;
        result.reply += "\n🤖 Mode: AUTOPILOT";
    } else if (modeStr == "manual" || modeStr == "MANUAL") {
        result.modeChange = AiChatResult::ModeChange::Manual;
        result.reply += "\n🤖 Mode: MANUAL";
    }

    int f = contentDoc["frequency"] | 0;
    int v = contentDoc["coreVoltage"] | 0;

    if (f > 0 && v > 0) {
        if (Limits::isValidSetting(f, v)) {
            miner.applySettings(f, v, "AI chat");
            result.settingsApplied = true;
            result.reply += "\n⚙️ AI Applied: " + std::to_string(f) + " MHz / " + std::to_string(v) + " mV";
        } else {
            result.reply += "\n🚫 AI suggested invalid parameters (" + std::to_string(f) + " MHz / " + std::to_string(v) + " mV) — change rejected.";
        }
    }

    // Remember the raw model output so the next question sees this exchange
    chatMemory.addExchange(question, content);

    return result;
}
