# 📜 Development history: AxePilot

A step-by-step chronicle of how this project was created and evolved. (Phases 1–6 predate the project's rename to AxePilot.)

## Phase 1: Base architecture and setup
- Created an empty PlatformIO project with the Arduino framework for ESP32.
- Configured the profile for the ESP32-2432S028R (Cheap Yellow Display).
- Added the `TFT_eSPI` library for graphics output.
- Implemented a basic `main.cpp` with a WiFi connection.

## Phase 2: Reading telemetry (Bitaxe API)
- Built the `BitaxeController` class talking to the miner's local HTTP API.
- Created the `BitaxeData` struct for the current readings (temperature, hashrate, voltage, frequency).
- Set up regular polling (every 2 seconds).
- Rendered the data on the CYD screen.

## Phase 3: Memory optimization and safety
- **Problem**: constant `String` concatenation caused heap fragmentation on the ESP32 and subsequent crashes.
- **Solution**: the code was rewritten to strictly avoid `String` in the main loop.
- Text rendering switched to the classic C approach: `snprintf` with `char buf[64]` buffers.

## Phase 4: Remote monitoring (Telegram bot)
- Built the `TelegramNotifier` class.
- Integrated the Telegram API for sending messages to the chat.
- Added a protection mechanism: when the Bitaxe temperature exceeds the threshold, the bot automatically sends a `⚠️ CRITICAL` message.
- Implemented non-blocking polling (`getUpdates?timeout=0`) so the bot listens for commands without freezing the UI.

## Phase 5: AI (DeepSeek integration)
- **Problem**: the standard Arduino `HTTPClient` mishandled chunked transfer encoding from modern APIs, so DeepSeek responses arrived empty or truncated.
- **Solution**: a custom `EspHttpClient` that reads the stream manually and parses the chunks correctly.
- Built the `DeepSeekOptimizer` that feeds telemetry into a system prompt and gets tuning advice back.

## Phase 6: Hybrid autopilot and control
- **Dynamic auto-tuning**: autopilot logic added. When `isTooCold` (≤ 60°C) the AI raises the frequency; when `isOverheating` the AI lowers voltage and frequency. A 30 s cooldown timer added.
- **CYD touch (emergency throttle)**:
  - **Problem**: the screen tried to read touches over the shared SPI bus, but the XPT2046 on CYD boards is wired to a separate bus.
  - **Solution**: added the `XPT2046_Touchscreen` library and initialized a separate global `SPI` (pins 25, 39, 32, 33).
  - A screen press now instantly switches the device to `MANUAL` and drops the frequency to 400 MHz.
- **Natural-language Telegram interface**: the AI outputs strict JSON (with safe `| 0` fallbacks via ArduinoJson v7) and can change frequency, voltage and the AUTO/MANUAL mode during a plain conversation.
- Added a Telegram menu with clickable commands (`/status`, `/auto`, `/manual`, `/help`).

---

## After the code review (2026-07-02 → 2026-07-05)
- Repaired the native test suite (it did not compile) and grew it to 66 cases; CI on GitHub Actions.
- Secrets moved to a gitignored `secrets.h`; git history scrubbed with `git filter-repo`.
- Multi-layer thermal protection (`SafetyGuard`, AI-failure fallback), validation of every settings change.
- Networking moved to a dedicated FreeRTOS task; TLS reuse for Telegram; watchdog.
- `/history`, `/bench`, `/fan`, `/restart`, daily digest, AI chat conversation memory.
- Project renamed to **AxePilot** and published: https://github.com/rizon0503/axepilot

## Phase 7: Touch, screens, and reliability hardening (2026-07-06 → 2026-07-08)
- **Problem**: the CYD's resistive touch was uncalibrated, so the emergency-throttle button's hit-test was unreliable. **Solution**: calibrated against measured XPT2046 corner readings; added a Controls screen (freq/volt presets, AUTO/MANUAL toggle, restart) and a Diagnostics screen (uptime, reset reason, WiFi RSSI, heap, interventions), both reachable via tab buttons.
- **Problem**: full-screen redraws every ~500ms caused visible flicker. **Solution**: per-line dirty-checking in the new `UiRenderer` (extracted from `main.cpp` so it's testable via an `IDisplay` mock) — a line only repaints if its text or color actually changed.
- **Problem**: DeepSeek's chunked HTTP responses sometimes arrived truncated, and any HTTP failure was indistinguishable from "the miner reported zero everything." **Solution**: a natively-tested chunked-transfer decoder, and a structured `HttpResult` (ok/statusCode/body/errorMessage) replacing a fragile `{error_...}` string convention.
- **Problem**: a hardcoded Bitaxe IP would strand the controller after a DHCP reassignment, and the lifetime intervention counter in NVS was silently overwriting itself every reboot instead of accumulating. **Solution**: mDNS discovery (`bitaxe.local`) with a static-IP fallback; fixed the counter to track a baseline + this-boot's count.
- **Problem**: the daily digest fired immediately on every restart instead of once a day. **Solution**: NTP time sync feeding real timestamps into `/history` and a fixed-UTC-hour digest schedule — plus a follow-up fix for a related bug where the digest still fired early the very first time NTP synced after boot.
- Root CAs pinned for Telegram and DeepSeek (`RootCerts.h`) instead of `WiFiClientSecure::setInsecure()`.

## Phase 8: Sparklines, OTA, Web UI, and a security pass (2026-07-08 → 2026-07-09)
- Added temperature/hashrate sparklines to the Main screen (backed by the existing `TelemetryHistory` ring buffer), then labeled them with a "T"/"H" letter after live feedback that color alone didn't make them clear.
- **CI releases**: every push to `master` now publishes a versioned GitHub Release (`vYYYY.MM.DD.<run_number>`) with the compiled `firmware.bin` — built from placeholder secrets, so it's a rollback reference point, not a flashable binary.
- **OTA over WiFi**: a custom two-slot partition table (reclaiming an unused SPIFFS region grew flash headroom too) plus an ArduinoOTA receiver — `pio run -e esp32-cyd-ota -t upload` replaces the COM cable for every flash after the first. Verified live end-to-end, including a follow-up fix for an espota auth-flag wiring bug and the one-time Windows Firewall rule the transfer needs.
- **Web UI**: a small embedded HTTP server serves `http://axepilot.local/` — a read-only dashboard mirroring the Main screen's telemetry and sparklines, built from the same data snapshot the physical screen renders.
- **Audit pass**: a systematic implementation review (`pio check`/cppcheck, now gating CI) and a security review across every network-facing surface — found and fixed a silent TLS-validation gap for custom `AI_BASE_URL` endpoints, and produced [SECURITY.md](SECURITY.md) documenting the threat model and every accepted risk.
- Project licensed under [PolyForm Strict 1.0.0](LICENSE.md).
- Native test suite: 66 → 182 cases.

*The full chronicle from this point onward lives in the git history and closed PRs.*
