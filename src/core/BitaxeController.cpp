#include "core/BitaxeController.h"
#include "core/Limits.h"
#include <ArduinoJson.h>
#include <cstdio>

BitaxeController::BitaxeController(IHttpClient& httpClient, ISystemTime& sysTime, const std::string& ipAddress)
    : httpClient(httpClient), sysTime(sysTime), ipAddress(ipAddress), lastUpdate(0) {
    currentData = BitaxeData{};
}

void BitaxeController::update() {
    uint32_t now = sysTime.millis();
    if (now - lastUpdate >= 2000 || lastUpdate == 0) { // Update every 2 seconds
        lastUpdate = now;
        
        std::string url = "http://" + ipAddress + "/api/system/info";
        std::string response = httpClient.get(url);
        
        if (!response.empty() && response.size() <= Limits::MAX_JSON_RESPONSE_BYTES) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                currentData.temperature = doc["temp"] | 0.0f;
                currentData.hashrate = doc["hashRate"] | 0.0f;
                currentData.coreVoltage = doc["coreVoltage"] | 0;
                currentData.frequency = doc["frequency"] | 0;
                currentData.power = doc["power"] | 0.0f;
                currentData.vrTemp = doc["vrTemp"] | 0.0f;
                currentData.fanSpeedPercent = doc["fanspeed"] | 0;
                currentData.fanRpm = doc["fanrpm"] | 0;
                currentData.sharesAccepted = doc["sharesAccepted"] | 0;
                currentData.sharesRejected = doc["sharesRejected"] | 0;
                currentData.uptimeSeconds = doc["uptimeSeconds"] | 0;
                const char* bestDiff = doc["bestDiff"] | "";
                snprintf(currentData.bestDiff, sizeof(currentData.bestDiff), "%s", bestDiff);
                currentData.isOverheating = currentData.temperature >= Limits::TEMP_MAX;
                // temp == 0.0 means the sensor reading is invalid — treat as neither hot nor cold
                currentData.isTooCold = (currentData.temperature > 0.0f && currentData.temperature <= Limits::TEMP_COLD);
            }
        }
    }
}

BitaxeData BitaxeController::getData() const {
    return currentData;
}

void BitaxeController::applySettings(int frequency, int coreVoltage, const char* source) {
    interventionLog.add(sysTime.millis(), source, frequency, coreVoltage);

    JsonDocument cmd;
    cmd["frequency"] = frequency;
    cmd["coreVoltage"] = coreVoltage;
    cmd["autofanspeed"] = 1;
    std::string payload;
    serializeJson(cmd, payload);
    httpClient.patch("http://" + ipAddress + "/api/system", payload, "");
}

void BitaxeController::setFanSpeed(int percent) {
    JsonDocument cmd;
    cmd["autofanspeed"] = 0;
    cmd["fanspeed"] = percent;
    std::string payload;
    serializeJson(cmd, payload);
    httpClient.patch("http://" + ipAddress + "/api/system", payload, "");
}

void BitaxeController::setAutoFan() {
    JsonDocument cmd;
    cmd["autofanspeed"] = 1;
    std::string payload;
    serializeJson(cmd, payload);
    httpClient.patch("http://" + ipAddress + "/api/system", payload, "");
}

void BitaxeController::restartMiner() {
    httpClient.post("http://" + ipAddress + "/api/system/restart", "", "");
}

const InterventionLog& BitaxeController::interventions() const {
    return interventionLog;
}
