#pragma once
#include "core/TelegramNotifier.h"
#include "core/DeepSeekOptimizer.h"
#include "core/BitaxeController.h"
#include "core/OperationMode.h"
#include "core/BitaxeData.h"
#include "core/TelemetryHistory.h"
#include "core/BenchmarkRunner.h"
#include "interfaces/ISystemInfo.h"
#include "interfaces/ISystemTime.h"
#include <string>

// Dispatches incoming Telegram messages: known slash-commands are handled
// locally, anything else is forwarded to the AI chat. Pure business logic —
// fully testable on the host.
class CommandRouter {
public:
    CommandRouter(TelegramNotifier& notifier, DeepSeekOptimizer& optimizer, BitaxeController& miner,
                  ISystemInfo& sysInfo, ISystemTime& sysTime, const TelemetryHistory& history,
                  BenchmarkRunner& benchmark);

    // Handles one message; may change `mode`.
    void handle(const std::string& msg, const BitaxeData& data, OperationMode& mode);

private:
    TelegramNotifier& notifier;
    DeepSeekOptimizer& optimizer;
    BitaxeController& miner;
    ISystemInfo& sysInfo;
    ISystemTime& sysTime;
    const TelemetryHistory& history;
    BenchmarkRunner& benchmark;
};
