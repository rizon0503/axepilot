#pragma once
#include "interfaces/IHttpClient.h"
#include <string>
#include <cstdint>

class TelegramNotifier {
public:
    TelegramNotifier(IHttpClient& httpClient, const std::string& botToken, const std::string& chatId);
    bool checkAndAlert(float temperature);
    
    std::string pollNewMessage(uint32_t currentMillis);
    void setupCommands();
    void sendMessage(const std::string& text);

private:
    IHttpClient& httpClient;
    std::string botToken;
    std::string chatId;
    bool alertSent;
    
    uint32_t lastPollTime;
    int lastUpdateId;
};
