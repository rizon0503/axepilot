# ⛏️ AxePilot

An autonomous controller and monitor for the **Bitaxe Gamma** ASIC miner (Axe OS, BM1370), running on an **ESP32-2432S028R** — the "Cheap Yellow Display" (CYD).

The device reads the miner's telemetry over its local HTTP API, renders it on the touchscreen, is controlled through a Telegram bot, and uses the **DeepSeek API** as an AI autopilot that tunes frequency and voltage based on temperature and its trend.

## Features

- 📟 **CYD screen**: temperature, hashrate, voltage, frequency, power/fan, operation mode; backlight auto-off after 15 s.
- 🚨 **Emergency throttle from the screen**: tapping the on-screen button drops the miner to a safe 400 MHz / 1000 mV.
- 🛡️ **SafetyGuard (no cloud)**: at ≥72°C it locally forces safe settings — even if the internet or DeepSeek is down. After 3 failed AI calls while overheating, the same local fallback kicks in.
- 🤖 **AI autopilot**: on overheating (≥70°C) or underperformance (≤60°C) DeepSeek receives the telemetry plus a 10-minute history with the trend and returns new settings; every value passes strict validation (`Limits.h`).
- 💬 **Telegram bot**: `/status` (extended stats with GH/W and trend), `/auto`, `/manual`, `/set <freq> <volt>`, `/fan <25-100>|auto`, `/restart`, `/history` (intervention journal), `/bench` (preset benchmark with a GH/W verdict), `/esp` (controller diagnostics), `/help`, plus free-form chat handled by the AI with conversation memory. The bot obeys only the configured chat id.
- 📅 **Daily digest**: average/min/max temperature, hashrate, power, GH/W, shares delta and intervention count, once every 24 h.
- 💾 **NVS**: the AUTO/MANUAL mode survives reboots.
- 🐕 **Watchdog**: a hung network stack reboots the controller instead of bricking it.

## Quick start

```bash
# 1. Environment
python -m venv .venv
.venv\Scripts\pip install platformio      # Windows

# 2. Secrets
copy include\secrets.h.example include\secrets.h
#    ...fill in your WiFi, miner IP, bot token, chat id and DeepSeek key

# 3. Tests (host-side, no hardware required)
.venv\Scripts\pio test -e native

# 4. Build and flash (serial — required for the very first flash)
.venv\Scripts\pio run -e esp32-cyd -t upload --upload-port COM4

# 5. Later reflashes can go over WiFi instead (no cable needed)
.venv\Scripts\pio run -e esp32-cyd-ota -t upload
```

## OTA updates

After the initial serial flash, the firmware can be updated over WiFi: the device runs an ArduinoOTA receiver (`axepilot.local`, port 3232), and the `esp32-cyd-ota` environment pushes a locally built image to it. The upload is authenticated with `OTA_PASSWORD` from `include/secrets.h` (read automatically by `ota_auth.py`). During the transfer the screen shows a progress bar; the device reboots into the new firmware on success and keeps running the old one if the transfer fails.

Note that OTA only works with locally built images — the binaries CI attaches to releases are compiled with placeholder secrets and would boot without your WiFi credentials.

## Architecture

```
main.cpp ── composition and the UI loop only (core 1)
networkTask (core 0, FreeRTOS) ── all networking: telemetry, Telegram, DeepSeek
        │
        ├── core/ ──────────── hardware-independent business logic
        │     BitaxeController   telemetry + the single applySettings() entry point
        │     SafetyGuard        local emergency protection (72°C)
        │     DeepSeekOptimizer  AI autopilot + chat (AiChatResult, ChatMemory)
        │     CommandRouter      Telegram command dispatcher
        │     TelegramNotifier   sending/polling messages, thermal alerts
        │     TelemetryHistory   10-minute ring buffer, °C/min trend
        │     BenchmarkRunner    freq/volt preset benchmark (GH/W)
        │     InterventionLog    journal of the last settings changes
        │     DailyStats         daily digest accumulators
        │     Limits.h           thermal thresholds and safe tuning ranges
        │
        ├── interfaces/ ─────── IHttpClient, IDisplay, ISystemTime,
        │                       ISystemInfo, ISettingsStore
        │
        └── hal/ ────────────── ESP32 implementations (TFT_eSPI, XPT2046,
                                HTTPClient+TLS, NVS Preferences)
```

All logic in `core/` is tested natively (122 Unity test cases): `pio test -e native`. Hardware is mocked through the `interfaces/` layer.

## Settings safety

Every frequency/voltage change — whether from the AI, the `/set` command or the button — goes through `Limits::isValidSetting()`: 400–625 MHz, 1000–1250 mV. Thresholds: 70°C — autopilot throttle and alert, 72°C — local emergency throttle with no AI involved. Manual fan speed is floored at 25%.

## Hardware (CYD, ESP32-2432S028R)

- ILI9341 display on SPI (pins in `platformio.ini`, `TFT_eSPI` build flags).
- XPT2046 touch on a **separate** SPI bus: `SPI.begin(25, 39, 32, 33)` — do not use the built-in `TFT_eSPI` touch.
- The touch is calibrated against measured raw XPT2046 corner readings ([#5](https://github.com/rizon0503/axepilot/issues/5)); the emergency throttle only fires when the calibrated touch lands inside its button rectangle.

## Development

- Roadmap: [GitHub Issues](https://github.com/rizon0503/axepilot/issues), grouped into [milestones](https://github.com/rizon0503/axepilot/milestones)
- `master` is protected: changes land via PRs with a required green `build-and-test` check
- CI runs the native test suite and the ESP32 build on every push and pull request
- [TODO.md](TODO.md) — archive of shipped work; [HISTORY.md](HISTORY.md) — early development chronicle
