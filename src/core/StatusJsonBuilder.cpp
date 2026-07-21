#include "core/StatusJsonBuilder.h"
#include <ArduinoJson.h>

std::string StatusJsonBuilder::build(const BitaxeData& data, OperationMode mode, bool wifiOk,
                                      const float* tempHistory, size_t tempHistoryCount,
                                      const float* hashHistory, size_t hashHistoryCount) {
    JsonDocument doc;
    doc["temperature"] = data.temperature;
    doc["hashrate"] = data.hashrate;
    doc["coreVoltage"] = data.coreVoltage;
    doc["frequency"] = data.frequency;
    doc["power"] = data.power;
    doc["fanSpeedPercent"] = data.fanSpeedPercent;
    doc["fanRpm"] = data.fanRpm;
    doc["mode"] = mode == OperationMode::AUTOPILOT ? "AUTO" : "MANUAL";
    doc["wifiOk"] = wifiOk;
    doc["isOverheating"] = data.isOverheating;

    JsonArray tempArr = doc["tempHistory"].to<JsonArray>();
    for (size_t i = 0; i < tempHistoryCount; i++) {
        tempArr.add(tempHistory[i]);
    }
    JsonArray hashArr = doc["hashHistory"].to<JsonArray>();
    for (size_t i = 0; i < hashHistoryCount; i++) {
        hashArr.add(hashHistory[i]);
    }

    std::string json;
    serializeJson(doc, json);
    return json;
}
