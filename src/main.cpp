#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp32-hal-log.h>
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
#include "core/TouchMapper.h"
#include "core/AppState.h"
#include "core/ControlsScreen.h"
#include "core/UiRenderer.h"

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
UiRenderer uiRenderer(display);

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
// Controls-screen actions: set by UI touch, consumed by the network task
// (all Bitaxe HTTP calls live there — see networkTask()).
static std::atomic<bool> presetRequested{false};
static std::atomic<int> presetFrequency{0};
static std::atomic<int> presetCoreVoltage{0};
static std::atomic<bool> restartRequested{false};
// Applied on the network task, not written directly from the UI thread —
// the Telegram command handler below does a non-atomic load-mutate-store of
// currentMode, so a concurrent direct write from loop() could be clobbered.
static std::atomic<bool> modeToggleRequested{false};

enum class Screen { MAIN, CONTROLS, DIAGNOSTICS };

// UI-only state (touched exclusively from loop())
AppState::ScreenPower screenPower;
uint32_t lastUiRefresh = 0;
AppState::RestoreTimer throttleRestore;
AppState::RestoreTimer restartRestore;
Screen currentScreen = Screen::MAIN;
bool touchWasDown = false;       // edge-detects Controls-screen taps; see touchStarted in loop()

// Avoids WiFi.localIP().toString() (Arduino String) on the network task's
// hot loop — snprintf into a caller buffer instead, per this project's
// no-String-in-hot-paths rule.
static void formatIp(char* buf, size_t len, IPAddress ip) {
    snprintf(buf, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

// Gathers the live ESP32 diagnostics into UiRenderer's data shape — the
// rendering itself lives in core/UiRenderer (#14) so it's testable via an
// IDisplay mock; this glue reads the hardware-backed singletons UiRenderer
// has no reason to depend on.
static void renderDiagnosticsScreen() {
    UiRenderer::DiagnosticsData diag;
    diag.uptimeSeconds = sysTime.millis() / 1000;
    diag.resetReason = sysInfo.resetReason();
    diag.resetCount = rebootStats.resetCount();
    diag.wifiRssi = sysInfo.wifiRssi();
    diag.wifiReconnectCount = sysInfo.wifiReconnectCount();
    diag.freeHeapBytes = sysInfo.freeHeapBytes();
    diag.minFreeHeapBytes = sysInfo.minFreeHeapBytes();
    diag.maxAllocBytes = sysInfo.maxAllocBytes();
    diag.interventionTotal = rebootStats.interventionTotal();
    uiRenderer.renderDiagnosticsScreen(diag);
}

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
    log_i("Network task started, watchdog armed (180s timeout)");

    OperationMode persistedMode = currentMode.load();
    for (;;) {
        esp_task_wdt_reset();
        if (WiFi.status() != WL_CONNECTED) {
            if (wifiConnected.load()) {
                sysInfo.noteWifiReconnect(); // count each connection loss once
                log_w("WiFi disconnected, reconnecting...");
            }
            wifiConnected = false;
            WiFi.disconnect();
            WiFi.reconnect();
            uint32_t start = sysTime.millis();
            while (WiFi.status() != WL_CONNECTED && sysTime.millis() - start < 5000) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            if (WiFi.status() != WL_CONNECTED) {
                log_w("WiFi reconnect attempt failed, retrying (drops so far: %u)", (unsigned)sysInfo.wifiReconnectCount());
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue; // Skip API calls until reconnected
            }
            char ipStr[16];
            formatIp(ipStr, sizeof(ipStr), WiFi.localIP());
            log_i("WiFi reconnected: %s", ipStr);
        }
        // Start (or restart) NTP sync exactly once per "became connected"
        // transition — covers the initial connect, a connect that happens
        // after setup()'s WiFi wait already timed out, and any later
        // reconnect after a drop. Cheap enough to not need throttling
        // beyond that.
        if (!wifiConnected.load()) {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC, no DST offset
        }
        wifiConnected = true;

        controller.update();
        BitaxeData data = controller.getData();
        portENTER_CRITICAL(&dataMux);
        sharedData = data;
        portEXIT_CRITICAL(&dataMux);
        history.record(data, sysTime.millis());
        dailyStats.record(data, sysTime.millis());

        uint32_t lifetimeInterventionsBefore = rebootStats.interventionTotal();
        rebootStats.tick(sysTime.millis() / 1000, controller.interventions().totalCount());
        uint32_t lifetimeInterventionsNow = rebootStats.interventionTotal();
        if (lifetimeInterventionsNow != lifetimeInterventionsBefore) {
            char journal[768];
            controller.interventions().format(journal, sizeof(journal), sysTime.millis());
            log_i("Intervention recorded (lifetime total: %u):\n%s", (unsigned)lifetimeInterventionsNow, journal);
        }

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

        // Frequency/voltage preset tapped on the Controls screen
        if (presetRequested.exchange(false)) {
            currentMode = OperationMode::MANUAL;
            int freq = presetFrequency.load();
            int volt = presetCoreVoltage.load();
            controller.applySettings(freq, volt, "CYD preset");
            char presetMsg[96];
            snprintf(presetMsg, sizeof(presetMsg), "⚙️ Preset applied via screen.\nMode: MANUAL, Freq: %d, Volt: %d", freq, volt);
            notifier.sendMessage(presetMsg);
        }

        // Miner restart requested from the Controls screen
        if (restartRequested.exchange(false)) {
            controller.restartMiner();
            notifier.sendMessage("♻️ Miner restart requested via screen. Hashrate will recover in ~1-2 min.");
        }

        // AUTO/MANUAL toggle tapped on the Controls screen
        if (modeToggleRequested.exchange(false)) {
            OperationMode newMode = currentMode.load() == OperationMode::AUTOPILOT ? OperationMode::MANUAL : OperationMode::AUTOPILOT;
            currentMode = newMode;
            notifier.sendMessage(newMode == OperationMode::AUTOPILOT
                                      ? "🔄 Mode switched to AUTO via screen."
                                      : "🔄 Mode switched to MANUAL via screen.");
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
    log_i("Boot: reset reason=%s, total resets=%u", sysInfo.resetReason(), (unsigned)rebootStats.resetCount());
    display.init();
    display.drawText(10, 10, "Connecting to WiFi...", TFT_WHITE);

    WiFi.begin(ssid, password);
    uint32_t wifiWaitStart = sysTime.millis();
    while (WiFi.status() != WL_CONNECTED && sysTime.millis() - wifiWaitStart < WIFI_SETUP_TIMEOUT_MS) {
        sysTime.delay(500);
    }

    display.clear();
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        char ipStr[16];
        formatIp(ipStr, sizeof(ipStr), WiFi.localIP());
        log_i("WiFi connected: %s (RSSI %d dBm)", ipStr, sysInfo.wifiRssi());
        display.drawText(10, 10, "WiFi Connected!", TFT_GREEN);
    } else {
        log_w("WiFi connect timed out after %ums, retrying in background", (unsigned)WIFI_SETUP_TIMEOUT_MS);
        display.drawText(10, 10, "WiFi not found.", TFT_RED);
        display.drawText(10, 40, "Retrying in background...", TFT_WHITE);
    }
    sysTime.delay(1000);

    // Draw Throttle Button (Bottom Bar) + Controls-screen tab; clears the
    // screen first (see UiRenderer::renderMainScreenChrome()).
    uiRenderer.renderMainScreenChrome();

    // Set up Telegram Bot Menu — pointless without connectivity; the network
    // task doesn't repeat this call, but the bot menu isn't safety-critical.
    // NTP sync is started from the network task instead (see networkTask()),
    // so it isn't missed if WiFi connects only after this timeout window.
    if (WiFi.status() == WL_CONNECTED) {
        notifier.setupCommands();
    }
    screenPower.onTouch(true, sysTime.millis()); // primes the idle-timeout clock to "now"

    // Networking on core 0 (where the WiFi stack already lives); the Arduino
    // loop() with UI/touch stays on core 1. TLS needs a generous stack.
    xTaskCreatePinnedToCore(networkTask, "net", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
    uint32_t now = sysTime.millis();

    // UI Update, throttled to twice a second (snprintf to prevent heap fragmentation).
    // Only the Main screen shows live telemetry; the Controls screen is static.
    if (currentScreen == Screen::MAIN && (lastUiRefresh == 0 || now - lastUiRefresh >= 500)) {
        lastUiRefresh = now;
        BitaxeData data = readSharedData();
        OperationMode mode = currentMode.load();
        uiRenderer.renderTelemetry(data, mode, wifiConnected.load());
    }

    // Emergency Throttle Button & Screen Wakeup
    int tx, ty;
    bool touchedRaw = display.touched(tx, ty);
    // false while waking up or within the post-wake debounce window; see
    // AppState::ScreenPower. Implies isOn() whenever it returns true.
    bool touched = screenPower.onTouch(touchedRaw, now);
    if (screenPower.justWoke()) {
        display.setBacklight(true);
    }
    if (screenPower.tick(now)) {
        display.setBacklight(false);
    }

    // Controls-screen actions must fire once per physical tap, not once per
    // loop() iteration a tap happens to overlap (loop() runs every ~10ms, so
    // a single ~100-300ms tap would otherwise be dispatched many times —
    // e.g. toggling AUTO/MANUAL back and forth an indeterminate number of
    // times depending on exact tap duration).
    bool touchStarted = touched && !touchWasDown;
    touchWasDown = touched;

    // Snapshot the screen before dispatch: both screens' tab button shares
    // the same TAB_RECT, so a tap that switches Main -> Controls below would
    // otherwise also satisfy the Controls-dispatch block's currentScreen ==
    // CONTROLS check later in this same iteration (same touchStarted tap,
    // now on the just-switched-to screen) and immediately switch back,
    // leaving a half-drawn Controls screen that the next Main redraw then
    // painted telemetry text over.
    const Screen screenAtTouch = currentScreen;

    if (touched && screenAtTouch == Screen::MAIN) {
        if (!throttleRestore.isPending() && TouchMapper::isWithinRect(tx, ty, 0, 180, 320, 60)) {
            // The actual PATCH + Telegram message happen on the network task.
            throttleRequested = true;

            // Visual feedback; restored non-blockingly after 2 s
            uiRenderer.renderThrottleButton(UiRenderer::ThrottleState::TRIGGERED);
            throttleRestore.trigger(now, 2000);
        } else if (touchStarted && ControlsScreen::hitTestMainScreen(tx, ty) == ControlsScreen::Action::SWITCH_SCREEN) {
            currentScreen = Screen::CONTROLS;
            uiRenderer.renderControlsScreen(currentMode.load());
        }
    }

    if (throttleRestore.tick(now)) {
        // Only redraw over the Main screen — nothing to restore if the user
        // already navigated away to Controls within the 2 s window.
        if (currentScreen == Screen::MAIN) {
            uiRenderer.renderThrottleButton(UiRenderer::ThrottleState::NORMAL);
        }
    }

    if (touched && screenAtTouch == Screen::CONTROLS && touchStarted) {
        using ControlsScreen::Action;
        Action action = ControlsScreen::hitTest(tx, ty);
        switch (action) {
            case Action::SWITCH_SCREEN:
                currentScreen = Screen::DIAGNOSTICS;
                renderDiagnosticsScreen();
                break;
            case Action::TOGGLE_MODE: {
                OperationMode next = currentMode.load() == OperationMode::AUTOPILOT
                                          ? OperationMode::MANUAL
                                          : OperationMode::AUTOPILOT;
                modeToggleRequested = true; // applied on the network task; see the comment by its declaration
                uiRenderer.renderControlsScreen(next); // optimistic — matches what the network task applies next tick
                break;
            }
            case Action::RESTART:
                if (!restartRestore.isPending()) {
                    restartRequested = true;
                    uiRenderer.renderRestartButton(UiRenderer::RestartButtonState::SENT);
                    restartRestore.trigger(now, 2000);
                }
                break;
            case Action::PRESET_ECO:
            case Action::PRESET_BALANCED:
            case Action::PRESET_TURBO:
            case Action::PRESET_MAX: {
                ControlsScreen::Preset preset = ControlsScreen::presetFor(action);
                presetFrequency = preset.frequency;
                presetCoreVoltage = preset.coreVoltage;
                presetRequested = true; // network task also sets currentMode = MANUAL when it applies this
                uiRenderer.renderControlsScreen(OperationMode::MANUAL); // optimistic — matches what the network task applies next tick
                break;
            }
            case Action::NONE:
                break;
        }
    }

    if (restartRestore.tick(now)) {
        if (currentScreen == Screen::CONTROLS) {
            uiRenderer.renderRestartButton(UiRenderer::RestartButtonState::NORMAL);
        }
    }

    // Diagnostics has just the one button: its tab, which returns to Main.
    if (touched && screenAtTouch == Screen::DIAGNOSTICS && touchStarted &&
        TouchMapper::isWithinRect(tx, ty, ControlsScreen::TAB_RECT.x, ControlsScreen::TAB_RECT.y,
                                   ControlsScreen::TAB_RECT.w, ControlsScreen::TAB_RECT.h)) {
        currentScreen = Screen::MAIN;
        uiRenderer.resetTelemetryCache();
        lastUiRefresh = 0; // force an immediate redraw instead of waiting up to 500ms
        uiRenderer.renderMainScreenChrome();
    }

    sysTime.delay(10); // Yield; all periodic work is millis()-gated above
}
