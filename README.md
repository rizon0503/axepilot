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
- 📡 **OTA updates**: after the first serial flash, new firmware is pushed over WiFi (`pio run -e esp32-cyd-ota -t upload`) with an on-screen progress bar; a failed transfer keeps the old firmware running.
- 🌐 **Web UI**: `http://axepilot.local/` mirrors the CYD's Main screen from any device on the LAN — same telemetry and sparklines, read-only, no login.

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

After the initial serial flash, the firmware can be updated over WiFi: the device runs an ArduinoOTA receiver (`axepilot.local`, port 3232), and the `esp32-cyd-ota` environment pushes a locally built image to it. The upload is authenticated with `OTA_PASSWORD` from `include/secrets.h` (read automatically by `ota_auth.py`, and it must match the password the running firmware was built with). During the transfer the screen shows a progress bar and touch input is suspended; the device reboots into the new firmware on success and keeps running the old one if the transfer fails. If mDNS resolution of `axepilot.local` fails on your network, pass the device address directly: `pio run -e esp32-cyd-ota -t upload --upload-port <IP>`.

Note that OTA only works with locally built images — the binaries CI attaches to releases are compiled with placeholder secrets and would boot without your WiFi credentials. Serial flashing over the COM port always remains available as a fallback (and is the only way to apply a partition-table change).

### One-time host setup on Windows

espota's transfer phase is a **device→host TCP connection** to a random port on the uploading machine (only the invitation/auth handshake is UDP). Windows Firewall blocks that inbound connection by default — the telltale symptom is `Authenticating...OK` followed by `No response from device`. Allow the project venv's Python once, from an elevated PowerShell:

```powershell
New-NetFirewallRule -DisplayName "espota-axepilot" -Direction Inbound `
  -Program "<path-to-repo>\.venv\Scripts\python.exe" `
  -Action Allow -Profile Any
```

The rule is scoped to that one binary rather than a port because espota picks a random host port per upload. Use `-Profile Private` instead of `Any` if Windows categorizes your WiFi network as *Private* (check with `Get-NetConnectionProfile`).

## Web UI

`http://axepilot.local/` (or the device's IP) serves a small read-only dashboard: the same telemetry, mode and temperature/hashrate sparklines as the CYD's Main screen, polled from `GET /api/status` every 2 s. It's LAN-only with no authentication — don't port-forward it. Controls-screen actions (presets, mode toggle, restart) aren't exposed yet; see [#70](https://github.com/rizon0503/axepilot/issues/70) for that follow-up.

Two endpoints, both `GET`, no auth, LAN-only, served directly from the device (port 80):

| Endpoint       | Returns |
|----------------|---------|
| `/`            | The dashboard page (`text/html`) |
| `/api/status`  | Live telemetry (`application/json`), built by `StatusJsonBuilder` from the same snapshot the CYD screen renders |

`/api/status` example response:

```json
{
  "temperature": 65.9,
  "hashrate": 1000.9,
  "coreVoltage": 1150,
  "frequency": 500,
  "power": 20.1,
  "fanSpeedPercent": 0,
  "fanRpm": 3827,
  "mode": "AUTO",
  "wifiOk": true,
  "isOverheating": false,
  "tempHistory": [65.1, 65.4, 65.9],
  "hashHistory": [948.9, 970.2, 1000.9]
}
```

Field notes:
- `temperature` (°C), `hashrate` (GH/s, raw — the dashboard converts to TH/s client-side above 9999), `coreVoltage` (mV), `frequency` (MHz), `power` (W).
- `fanSpeedPercent` is `0` in autofan mode — read `fanRpm` instead, same convention as the CYD screen and the Telegram `/status` command.
- `mode` is `"AUTO"` or `"MANUAL"`; `wifiOk` reflects the ESP32's own WiFi connection, not the Bitaxe's.
- `tempHistory`/`hashHistory` are oldest-first, up to `TelemetryHistory::CAPACITY` (20 samples, ~30 s apart — a 10-minute window), same arrays the Main screen's sparklines draw.

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
        │     UiRenderer         all screen rendering (Main/Controls/Diagnostics/OTA)
        │     StatusJsonBuilder  Web UI's /api/status JSON body
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

All logic in `core/` is tested natively (182 Unity test cases): `pio test -e native`. Hardware is mocked through the `interfaces/` layer.

## Settings safety

Every frequency/voltage change — whether from the AI, the `/set` command or the button — goes through `Limits::isValidSetting()`: 400–625 MHz, 1000–1250 mV. Thresholds: 70°C — autopilot throttle and alert, 72°C — local emergency throttle with no AI involved. Manual fan speed is floored at 25%.

## Hardware (CYD, ESP32-2432S028R)

- ILI9341 display on SPI (pins in `platformio.ini`, `TFT_eSPI` build flags).
- XPT2046 touch on a **separate** SPI bus: `SPI.begin(25, 39, 32, 33)` — do not use the built-in `TFT_eSPI` touch.
- The touch is calibrated against measured raw XPT2046 corner readings ([#5](https://github.com/rizon0503/axepilot/issues/5)); the emergency throttle only fires when the calibrated touch lands inside its button rectangle.

## Development

- Roadmap: [GitHub Issues](https://github.com/rizon0503/axepilot/issues), grouped into [milestones](https://github.com/rizon0503/axepilot/milestones)
- Threat model and accepted security trade-offs: [SECURITY.md](SECURITY.md)
- `master` is protected: changes land via PRs with a required green `build-and-test` check
- CI runs the native test suite and the ESP32 build on every push and pull request
- [TODO.md](TODO.md) — archive of shipped work; [HISTORY.md](HISTORY.md) — early development chronicle

## License

[PolyForm Strict License 1.0.0](LICENSE.md) — source-available, not open source. In short: reading, studying, and any **noncommercial** use (personal projects, research, running it on your own hardware) is permitted; **commercial use, distribution, and modifications/derivative works are not**, without the copyright holder's written permission.
