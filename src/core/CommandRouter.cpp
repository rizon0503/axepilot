#include "core/CommandRouter.h"
#include "core/Limits.h"
#include <cstdio>

CommandRouter::CommandRouter(TelegramNotifier& notifier, DeepSeekOptimizer& optimizer, BitaxeController& miner,
                             ISystemInfo& sysInfo, ISystemTime& sysTime, const TelemetryHistory& history,
                             BenchmarkRunner& benchmark)
    : notifier(notifier), optimizer(optimizer), miner(miner), sysInfo(sysInfo), sysTime(sysTime),
      history(history), benchmark(benchmark) {}

void CommandRouter::handle(const std::string& msg, const BitaxeData& data, OperationMode& mode) {
    if (msg == "/mode auto" || msg == "/auto") {
        mode = OperationMode::AUTOPILOT;
        notifier.sendMessage("✅ Mode switched to AUTOPILOT.\nI'm now watching the temperature.");
    }
    else if (msg == "/mode manual" || msg == "/manual") {
        mode = OperationMode::MANUAL;
        notifier.sendMessage("⚠️ Mode switched to MANUAL.\nAutopilot disabled.");
    }
    else if (msg == "/status") {
        char status[512];
        int len = snprintf(status, sizeof(status),
                 "📊 Current status:\n🌡️ Temperature: %.1f°C",
                 data.temperature);

        if (history.size() >= 2 && len > 0 && (size_t)len < sizeof(status)) {
            len += snprintf(status + len, sizeof(status) - len, " (trend %+.2f°C/min)", history.tempTrendPerMinute());
        }

        if (len > 0 && (size_t)len < sizeof(status)) {
            char efficiency[48] = "";
            if (data.power > 0.1f) {
                snprintf(efficiency, sizeof(efficiency), " (%.1f GH/W)", data.hashrate / data.power);
            }
            // fanspeed=0% means AxeOS runs the fan automatically
            char fan[32];
            if (data.fanSpeedPercent > 0) {
                snprintf(fan, sizeof(fan), "%d%% (%d rpm)", data.fanSpeedPercent, data.fanRpm);
            } else {
                snprintf(fan, sizeof(fan), "auto, %d rpm", data.fanRpm);
            }
            snprintf(status + len, sizeof(status) - len,
                 "\n⛏️ Hashrate: %.1f GH/s\n⚙️ %d MHz / %d mV\n⚡ Power: %.1f W%s\n🌀 Fan: %s\n📈 Shares: ✅%u / ❌%u\n🏆 Best diff: %s\n⏱️ Miner uptime: %uh %um\n🤖 Mode: %s",
                 data.hashrate, data.frequency, data.coreVoltage,
                 data.power, efficiency,
                 fan,
                 (unsigned)data.sharesAccepted, (unsigned)data.sharesRejected,
                 data.bestDiff[0] ? data.bestDiff : "-",
                 (unsigned)(data.uptimeSeconds / 3600), (unsigned)((data.uptimeSeconds % 3600) / 60),
                 (mode == OperationMode::AUTOPILOT ? "AUTOPILOT" : "MANUAL"));
        }
        notifier.sendMessage(status);
    }
    else if (msg == "/esp") {
        char espInfo[384];
        uint32_t uptimeSec = sysTime.millis() / 1000;
        snprintf(espInfo, sizeof(espInfo),
                 "💻 **ESP32 CYD Status**:\n\n"
                 "⏱️ Uptime: %02u:%02u:%02u\n"
                 "🔁 Reset reason: %s\n"
                 "📶 Wi-Fi RSSI: %d dBm (drops: %u)\n"
                 "🧠 Free Heap: %u KB (min: %u KB)\n"
                 "📦 Max Alloc: %u KB",
                 (unsigned)(uptimeSec / 3600), (unsigned)((uptimeSec % 3600) / 60), (unsigned)(uptimeSec % 60),
                 sysInfo.resetReason(),
                 sysInfo.wifiRssi(), (unsigned)sysInfo.wifiReconnectCount(),
                 (unsigned)(sysInfo.freeHeapBytes() / 1024), (unsigned)(sysInfo.minFreeHeapBytes() / 1024),
                 (unsigned)(sysInfo.maxAllocBytes() / 1024));
        notifier.sendMessage(espInfo);
    }
    else if (msg == "/fan auto") {
        miner.setAutoFan();
        notifier.sendMessage("🌀 Fan switched to auto mode.");
    }
    else if (msg.rfind("/fan ", 0) == 0) {
        int percent = -1;
        if (sscanf(msg.c_str(), "/fan %d", &percent) == 1) {
            if (percent >= Limits::FAN_MIN_PERCENT && percent <= 100) {
                miner.setFanSpeed(percent);
                char fanMsg[160];
                snprintf(fanMsg, sizeof(fanMsg), "🌀 Fan: %d%% (manual mode).\n⚠️ Any freq/volt change will switch it back to auto.", percent);
                notifier.sendMessage(fanMsg);
            } else {
                char fanMsg[96];
                snprintf(fanMsg, sizeof(fanMsg), "🚫 Allowed range: %d-100%%, or /fan auto", Limits::FAN_MIN_PERCENT);
                notifier.sendMessage(fanMsg);
            }
        } else {
            notifier.sendMessage("❌ Format: /fan <25-100> or /fan auto");
        }
    }
    else if (msg == "/restart") {
        miner.restartMiner();
        notifier.sendMessage("♻️ Restart command sent to the miner. Hashrate will recover in ~1-2 min.");
    }
    else if (msg == "/history") {
        if (miner.interventions().size() == 0) {
            notifier.sendMessage("📜 Journal is empty: no settings changes since startup.");
        } else {
            char log[768];
            miner.interventions().format(log, sizeof(log), sysTime.millis());
            notifier.sendMessage(std::string("📜 Recent interventions (newest first):\n\n") + log);
        }
    }
    else if (msg == "/bench" || msg == "/bench status") {
        notifier.sendMessage(benchmark.progress(sysTime.millis()));
    }
    else if (msg == "/bench start") {
        mode = OperationMode::MANUAL; // the autopilot must not fight the benchmark
        notifier.sendMessage(benchmark.start(sysTime.millis()));
    }
    else if (msg == "/bench stop") {
        notifier.sendMessage(benchmark.stop());
    }
    else if (msg == "/help") {
        notifier.sendMessage(
            "AxePilot help:\n\n"
            "🔹 /auto - Enable autopilot (throttles above 70°C, speeds up below 60°C)\n"
            "🔹 /manual - Disable autopilot\n"
            "🔹 /status - Get a quick summary\n"
            "🔹 /esp - Controller status\n"
            "🔹 /set <freq> <volt> - Force specific settings\n"
            "🔹 /history - Journal of recent settings changes\n"
            "🔹 /bench start|stop|status - Benchmark presets (~30 min)\n"
            "🔹 /fan <25-100> | /fan auto - Fan control\n"
            "🔹 /restart - Restart the miner\n\n"
            "💡 You can also just talk to me in plain language (e.g. 'set frequency to 550'), and I'll use AI to handle it!"
        );
    }
    else if (msg.rfind("/set ", 0) == 0) {
        int f = 0, v = 0;
        if (sscanf(msg.c_str(), "/set %d %d", &f, &v) == 2) {
            if (Limits::isValidSetting(f, v)) {
                mode = OperationMode::MANUAL;
                miner.applySettings(f, v, "/set");
                notifier.sendMessage("⚙️ MANUAL OVERRIDE applied:\nFreq: " + std::to_string(f) + " MHz\nVolt: " + std::to_string(v) + " mV");
            } else {
                char rangeMsg[128];
                snprintf(rangeMsg, sizeof(rangeMsg), "🚫 Invalid values. Freq: %d-%d MHz, Volt: %d-%d mV",
                         Limits::FREQ_MIN, Limits::FREQ_MAX, Limits::VOLT_MIN, Limits::VOLT_MAX);
                notifier.sendMessage(rangeMsg);
            }
        } else {
            notifier.sendMessage("❌ Invalid format. Use: /set <freq> <volt>");
        }
    }
    else {
        // Anything that is not a known command goes to the AI chat
        AiChatResult result = optimizer.askQuestion(msg, data);

        if (result.modeChange == AiChatResult::ModeChange::Auto) {
            mode = OperationMode::AUTOPILOT;
        } else if (result.modeChange == AiChatResult::ModeChange::Manual || result.settingsApplied) {
            mode = OperationMode::MANUAL;
        }

        notifier.sendMessage(result.reply);
    }
}
