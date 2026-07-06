# AxePilot - Continuation Prompt for AI Agents

**Target Audience:** AI Coding Assistants (Cursor, Copilot, Cline, etc.) loaded into a new IDE session.
**Purpose:** Provide full context, architectural decisions, and strict constraints for continuing development on this ESP32-CYD Bitaxe controller.

---

## 🎯 Project Overview
This project is a smart, autonomous hardware controller and monitor for a **Bitaxe Gamma** ASIC miner. It runs on an **ESP32-CYD (Cheap Yellow Display)**. 
It monitors the Bitaxe via its local HTTP API, provides a touchscreen UI, integrates a Telegram Bot for remote control, and uses **DeepSeek API** as an "Autopilot" to dynamically adjust frequency and voltage based on temperature.

---

## 🏗️ Architecture & Core Components

1.  **`main.cpp` (Orchestrator)**
    *   Handles WiFi connection, main non-blocking loop, and coordinates all subsystems.
    *   Renders the UI (text and the Emergency Throttle button).
    *   Manages `currentMode` (`AUTOPILOT` vs `MANUAL`).

2.  **`EspHttpClient` (HTTP Layer)**
    *   Custom wrapper around `HTTPClient`.
    *   *Crucial Detail:* Standard `HTTPClient` fails on chunked transfer encoding from DeepSeek. This wrapper explicitly handles headers and streams payload into `std::string` to avoid truncation. **Do not replace this with naive HTTPClient calls.**

3.  **`EspDisplay` (Hardware Abstraction - CYD)**
    *   Uses `TFT_eSPI` for drawing on the display.
    *   Uses `XPT2046_Touchscreen` for touch input.
    *   *Crucial Detail:* The CYD touch controller is on a **separate SPI bus** from the display. It must be initialized with the global `SPI.begin(25, 39, 32, 33)` before calling `ts.begin()`. Do NOT attempt to use `TFT_eSPI`'s internal touch functions.

4.  **`BitaxeController` (Telemetry)**
    *   Polls `http://<bitaxe_ip>/api/system/info` every 2 seconds.
    *   Sets flags: `isOverheating` (>= 70.0°C) and `isTooCold` (0 < temp <= 60.0°C; a 0.0°C reading is treated as a sensor glitch).
    *   All thresholds and safe freq/volt ranges live in `include/core/Limits.h` — the single source of truth used by every code path that changes miner settings.

5.  **`DeepSeekOptimizer` (The AI Autopilot)**
    *   Only fires if Autopilot is ON, a temperature threshold is breached, and 30 seconds have passed since the last optimization (cooldown).
    *   Sends telemetry and a strict system prompt to `https://api.deepseek.com/chat/completions`.
    *   Requires output in strict JSON format: `{"reply": "...", "frequency": X, "coreVoltage": Y, "mode": "auto"}`.
    *   *Crucial Detail:* Parses JSON using `contentDoc["frequency"] | 0` fallback instead of strict type checking to avoid crashes if AI hallucinates string types.

6.  **`TelegramNotifier` (Remote Control)**
    *   Uses Telegram Bot API (long polling via `getUpdates?timeout=0`).
    *   Handles commands: `/status`, `/auto`, `/manual`, `/set`, `/help`.
    *   Registers a persistent bot menu via `/setMyCommands` on boot.
    *   Any natural language message is forwarded to `DeepSeekOptimizer::askQuestion`.

---

## ⚠️ Strict Nuances & Constraints

1.  **Memory Management (NO `String` concatenation in loops)**
    *   To prevent heap fragmentation on the ESP32, avoid Arduino's `String` class in the `loop()`.
    *   **Rule:** Use `snprintf` with `char buf[256]` for formatting strings (especially UI updates).
    *   Use `std::string` for HTTP payloads and API returns.

2.  **Non-Blocking Logic (NO `delay()`)**
    *   Never use `delay()` inside `loop()` or class methods (except during initial `setup()`).
    *   **Rule:** Always use state machines or `sysTime.millis()` delta checks to schedule tasks (e.g., polling APIs, drawing UI).

3.  **JSON Parsing (ArduinoJson v7)**
    *   The project uses **ArduinoJson v7**. Note that v7 syntax is different from v6 (e.g., `containsKey` is deprecated; use `.is<T>()` or `.isNull()`).
    *   Always check `DeserializationError` before accessing a parsed document.

4.  **Mode Switching**
    *   If the user physically taps the CYD screen (Emergency Throttle), the device instantly drops to 400MHz/1000mV and forces `MANUAL` mode.
    *   If the AI is asked to change settings via Telegram chat, it must output `"mode": "manual"` (or "auto") in its JSON, which `main.cpp` detects to switch the internal state.

5.  **PlatformIO Environment**
    *   Environment: `esp32-cyd` (firmware) and `native` (host unit tests, `pio test -e native`).
    *   Libraries: `bodmer/TFT_eSPI`, `bblanchon/ArduinoJson`, `paulstoffregen/XPT2046_Touchscreen`.

6.  **Secrets**
    *   Credentials (WiFi, Telegram token, DeepSeek key, Bitaxe IP) live in `include/secrets.h`, which is gitignored. Copy `include/secrets.h.example` to get started. Never hardcode credentials in `main.cpp`.

7.  **Safety Validation**
    *   ANY parameter change (AI autopilot, AI chat, `/set` command) MUST pass `Limits::isValidSetting(freq, volt)` before being PATCHed to the Bitaxe. Never trust raw LLM output.
    *   The Telegram bot only obeys the chat id configured in `secrets.h` — messages from other chats are ignored.

---

## 🚀 How to Continue
When prompted to add a feature, strictly adhere to the established architecture. If adding UI elements, update `EspDisplay`. If adding Bitaxe commands, use `BitaxeController`. If extending AI logic, carefully modify the DeepSeek prompt without breaking the JSON output format.
