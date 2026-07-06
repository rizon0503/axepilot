#pragma once
#include "interfaces/IHttpClient.h"
#include "interfaces/ISystemTime.h"
#include "BitaxeController.h"
#include "BitaxeData.h"
#include "AiChatResult.h"
#include "TelemetryHistory.h"
#include "ChatMemory.h"
#include <string>
#include <cstdint>

class DeepSeekOptimizer {
public:
    // Works with any OpenAI-compatible chat completions endpoint;
    // defaults to DeepSeek.
    DeepSeekOptimizer(IHttpClient& httpClient, ISystemTime& sysTime, const std::string& apiKey, BitaxeController& miner,
                      const std::string& baseUrl = "https://api.deepseek.com/v1/chat/completions",
                      const std::string& model = "deepseek-chat");
    std::string optimize(const BitaxeData& data, const TelemetryHistory& history);
    AiChatResult askQuestion(const std::string& question, const BitaxeData& data);

private:
    IHttpClient& httpClient;
    ISystemTime& sysTime;
    std::string apiKey;
    BitaxeController& miner;
    std::string baseUrl;
    std::string model;
    uint32_t lastOptimizeTime;
    int aiFailureCount; // consecutive failed optimize() attempts while overheating
    ChatMemory chatMemory;
};
