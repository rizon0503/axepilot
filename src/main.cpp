#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <atomic>
#include "secrets.h"

// Any OpenAI-compatible endpoint can be set in secrets.h; DeepSeek by default
#ifndef AI_BASE_URL
#define AI_BASE_URL "https://api.deepseek.com/v1/chat/completions"
#endif
#ifndef AI_MODEL
#define AI_MODEL "deepseek-chat"
#endif
#include "hal/EspHttpClient.h"
#include "hal/EspDisplay.h"
#include "hal/EspSystemTime.h"
#include "hal/EspSystemInfo.h"
#include "hal/EspSettingsStore.h"
#include "core/BitaxeController.h"
#include "core/TelegramNotifier.h"
#include "core/DeepSeekOptimizer.h"
#include "core/SafetyGuard.h"
#include "core/CommandRouter.h"
#include "core/OperationMode.h"
#include "core/TelemetryHistory.h"
#include "core/BenchmarkRunner.h"
#include "core/DailyStats.h"
#include "core/RebootStats.h"
#include "core/Limits.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* bitaxeIp = BITAXE_IP;
const char* telegramBotToken = TELEGRAM_BOT_TOKEN;
const char* telegramChatId = TELEGRAM_CHAT_ID;
const char* deepseekApiKey = DEEPSEEK_API_KEY;

EspHttpClient httpClient;
EspDisplay display;
EspSystemTime sysTime;
EspSystemInfo sysInfo;
EspSettingsStore settingsStore;

BitaxeController controller(httpClient, sysTime, bitaxeIp);
TelegramNotifier notifier(httpClient, telegramBotToken, telegramChatId);
DeepSeekOptimizer optimizer(httpClient, sysTime, deepseekApiKey, controller, AI_BASE_URL, AI_MODEL);
SafetyGuard safetyGuard(controller);
TelemetryHistory history; // written only by the network task
BenchmarkRunner benchmark(controller);
DailyStats dailyStats;
RebootStats rebootStats(settingsStore); // rebootStats.begin() is called from setup(), not here — no NVS I/O before it's safe
CommandRouter router(notifier, optimizer, controller, sysInfo, sysTime, history, benchmark, rebootStats);

// ---- State shared between the network task (core 0) and the UI loop (core 1) ----
// BitaxeData is a small POD, so a spinlock-protected copy is enough.
static portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
static BitaxeData sharedData = {};
static std::atomic<OperationMode> currentMode{OperationMode::AUTOPILOT};
static std::atomic<bool> throttleRequested{false}; // set by UI touch, consumed by network task
static std::atomic<bool> wifiConnected{false};

// UI-only state (touched exclusively from loop())
uint32_t lastInteractionTime = 0;
bool isScreenOn = true;
uint32_t lastUiRefresh = 0;
uint32_t throttleRestoreAt = 0;  // 0 = button in normal state
uint32_t ignoreTouchUntil = 0;   // debounce window after waking the screen

static BitaxeData readSharedData() {
    portENTER_CRITICAL(&dataMux);
    BitaxeData copy = sharedData;
    portEXIT_CRITICAL(&dataMux);
    return copy;
}

// All HTTP(S) I/O lives here. A DeepSeek call can take tens of seconds —
// the UI loop on the other core keeps rendering and reacting to touch.
static void networkTask(void*) {
    // A hung TLS socket must reboot the controller, not brick it. 180 s
    // comfortably covers back-to-back 45 s LLM calls.
    esp_task_wdt_add(NULL);

    OperationMode persistedMode = currentMode.load();
    for (;;) {
        esp_task_wdt_reset();
        if (WiFi.status() != WL_CONNECTED) {
            if (wifiConnected.load()) {
                sysInfo.noteWifiReconnect(); // count each connection loss once
            }
            wifiConnected = false;
            WiFi.disconnect();
            WiFi.reconnect();
            uint32_t start = sysTime.millis();
            while (WiFi.status() != WL_CONNECTED && sysTime.millis() - start < 5000) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            if (WiFi.status() != WL_CONNECTED) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue; // Skip API calls until reconnected
            }
        }
        wifiConnected = true;

        controller.update();
        BitaxeData data = controller.getData();
        portENTER_CRITICAL(&dataMux);
        sharedData = data;
        portEXIT_CRITICAL(&dataMux);
        history.record(data, sysTime.millis());
        dailyStats.record(data, sysTime.millis());
        rebootStats.tick(sysTime.millis() / 1000, controller.interventions().totalCount());

        std::string digest = dailyStats.tick(sysTime.millis(), sysTime.epochSeconds(), controller.interventions().totalCount());
        if (!digest.empty()) {
            notifier.sendMessage(digest);
        }

        // Cloud-independent panic throttle — runs in EVERY mode, before anything else
        std::string panicAlert = safetyGuard.check(data);
        if (!panicAlert.empty()) {
            currentMode = OperationMode::MANUAL; // AI must not re-raise settings right away
            if (benchmark.isRunning()) {
                notifier.sendMessage(benchmark.stop());
            }
            notifier.sendMessage(panicAlert);
        }

        // Benchmark state machine (aborts itself on overheat)
        std::string benchMsg = benchmark.tick(data, sysTime.millis());
        if (!benchMsg.empty()) {
            notifier.sendMessage(benchMsg);
        }

        // Telegram thermal alerts
        notifier.checkAndAlert(data.temperature);

        // Emergency throttle requested via touchscreen
        if (throttleRequested.exchange(false)) {
            currentMode = OperationMode::MANUAL;
            controller.applySettings(Limits::FREQ_MIN, Limits::VOLT_MIN, "CYD button");
            notifier.sendMessage("🚨 EMERGENCY THROTTLE TRIGGERED VIA SCREEN 🚨\nMode: MANUAL\nFreq: 400, Volt: 1000");
        }

        // DeepSeek AI Autopilot (paused while a benchmark owns the settings)
        if (currentMode.load() == OperationMode::AUTOPILOT && !benchmark.isRunning()) {
            std::string aiAction = optimizer.optimize(data, history);
            if (!aiAction.empty()) {
                notifier.sendMessage(aiAction);
            }
        }

        // Interactive Telegram Chat & Manual Commands
        std::string newMsg = notifier.pollNewMessage(sysTime.millis());
        if (!newMsg.empty()) {
            OperationMode mode = currentMode.load();
            router.handle(newMsg, data, mode);
            currentMode.store(mode);
        }

        // Persist the mode so a power loss doesn't silently re-enable autopilot
        OperationMode modeNow = currentMode.load();
        if (modeNow != persistedMode) {
            settingsStore.saveMode(modeNow);
            persistedMode = modeNow;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// If WiFi is unavailable at boot, don't hang here forever — the network
// task (started below regardless) retries the connection continuously.
// Blocking setup() indefinitely would also leave the emergency throttle
// button undrawn and unresponsive.
constexpr uint32_t WIFI_SETUP_TIMEOUT_MS = 30000;

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(180, true); // panic + reboot if the network task hangs
    currentMode = settingsStore.loadMode(OperationMode::AUTOPILOT);
    rebootStats.begin();
    display.init();
    display.drawText(10, 10, "Connecting to WiFi...", TFT_WHITE);

    WiFi.begin(ssid, password);
    uint32_t wifiWaitStart = sysTime.millis();
    while (WiFi.status() != WL_CONNECTED && sysTime.millis() - wifiWaitStart < WIFI_SETUP_TIMEOUT_MS) {
        sysTime.delay(500);
        Serial.print(".");
    }

    display.clear();
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        display.drawText(10, 10, "WiFi Connected!", TFT_GREEN);
    } else {
        display.drawText(10, 10, "WiFi not found.", TFT_RED);
        display.drawText(10, 40, "Retrying in background...", TFT_WHITE);
    }
    sysTime.delay(1000);
    display.clear();

    // Draw Throttle Button (Bottom Bar)
    display.drawButton(0, 180, 320, 60, "EMERGENCY THROTTLE", TFT_RED);

    // Set up Telegram Bot Menu and start NTP sync — both pointless without
    // connectivity; if WiFi isn't up yet, epochSeconds() just keeps
    // returning 0 (see ISystemTime) and every caller already falls back to
    // uptime-relative behavior. configTime() doesn't block or repeat here:
    // once the SNTP client has WiFi, it syncs and keeps itself updated.
    if (WiFi.status() == WL_CONNECTED) {
        notifier.setupCommands();
        configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC, no DST offset
    }
    lastInteractionTime = sysTime.millis();

    // Networking on core 0 (where the WiFi stack already lives); the Arduino
    // loop() with UI/touch stays on core 1. TLS needs a generous stack.
    xTaskCreatePinnedToCore(networkTask, "net", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
    uint32_t now = sysTime.millis();

    // UI Update, throttled to twice a second (snprintf to prevent heap fragmentation)
    if (lastUiRefresh == 0 || now - lastUiRefresh >= 500) {
        lastUiRefresh = now;
        BitaxeData data = readSharedData();
        OperationMode mode = currentMode.load();
        char buf[64];

        snprintf(buf, sizeof(buf), "Temp: %.1f C", data.temperature);
        display.drawText(10, 10, buf, data.isOverheating ? TFT_RED : TFT_GREEN);

        // If hashrate is crazy high (like 86000 GH/s), we'll display it in TH/s to save screen space
        if (data.hashrate > 9999.0f) {
            snprintf(buf, sizeof(buf), "Hashrate: %.1f TH/s", data.hashrate / 1000.0f);
        } else {
            snprintf(buf, sizeof(buf), "Hashrate: %.1f GH/s", data.hashrate);
        }
        display.drawText(10, 40, buf, TFT_WHITE);

        snprintf(buf, sizeof(buf), "Volt: %d mV", data.coreVoltage);
        display.drawText(10, 70, buf, TFT_YELLOW);

        snprintf(buf, sizeof(buf), "Freq: %d MHz", data.frequency);
        display.drawText(10, 100, buf, TFT_CYAN);

        // In autofanspeed mode AxeOS reports fanspeed=0%, so RPM is the
        // meaningful number; show the percent only when it is set manually.
        if (data.fanSpeedPercent > 0) {
            snprintf(buf, sizeof(buf), "Pow: %.1fW Fan: %d%%", data.power, data.fanSpeedPercent);
        } else {
            snprintf(buf, sizeof(buf), "Pow: %.1fW Fan: %drpm", data.power, data.fanRpm);
        }
        display.drawText(10, 155, buf, TFT_WHITE);

        // Draw Mode Status + WiFi state. y=129 (not 130): Font 4 is 26px tall,
        // and the row below sits at y=155 — at y=130 this row's fillRect
        // would clip one pixel off the top of that row's text.
        if (!wifiConnected.load()) {
            display.drawText(10, 129, "Wi-Fi reconnecting...", TFT_RED);
        } else {
            snprintf(buf, sizeof(buf), "Mode: %s", mode == OperationMode::AUTOPILOT ? "AUTO" : "MANUAL");
            display.drawText(10, 129, buf, mode == OperationMode::AUTOPILOT ? TFT_GREEN : TFT_ORANGE);
        }
    }

    // Emergency Throttle Button & Screen Wakeup
    int tx, ty;
    bool touched = display.touched(tx, ty);

    if (touched) {
        lastInteractionTime = now;
        if (!isScreenOn) {
            display.setBacklight(true);
            isScreenOn = true;
            ignoreTouchUntil = now + 300; // Debounce to prevent accidental clicks when waking up
            touched = false; // Ignore touch this frame for UI logic
        } else if ((int32_t)(now - ignoreTouchUntil) < 0) {
            touched = false; // Still inside the wake-up debounce window
        }
    }

    if (isScreenOn && (now - lastInteractionTime > 15000)) {
        display.setBacklight(false);
        isScreenOn = false;
    }

    if (touched && isScreenOn && throttleRestoreAt == 0) {
        // Since resistive touch is uncalibrated, any firm press triggers the emergency throttle.
        // The actual PATCH + Telegram message happen on the network task.
        throttleRequested = true;

        // Visual feedback; restored non-blockingly after 2 s
        display.drawButton(0, 180, 320, 60, "THROTTLED!", TFT_ORANGE);
        throttleRestoreAt = now + 2000;
    }

    if (throttleRestoreAt != 0 && (int32_t)(now - throttleRestoreAt) >= 0) {
        display.drawButton(0, 180, 320, 60, "EMERGENCY THROTTLE", TFT_RED);
        throttleRestoreAt = 0;
    }

    sysTime.delay(10); // Yield; all periodic work is millis()-gated above
}
