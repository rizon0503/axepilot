# AxePilot - Continuation Prompt for AI Agents

**Target Audience:** AI Coding Assistants (Cursor, Copilot, Cline, etc.) loaded into a new IDE session.
**Purpose:** Provide full context, architectural decisions, and strict constraints for continuing development on this ESP32-CYD Bitaxe controller.

---

## 🎯 Project Overview
This project is a smart, autonomous hardware controller and monitor for a **Bitaxe Gamma** ASIC miner. It runs on an **ESP32-CYD (Cheap Yellow Display)**. 
It monitors the Bitaxe via its local HTTP API, provides a touchscreen UI, integrates a Telegram Bot for remote control, and uses **DeepSeek API** as an "Autopilot" to dynamically adjust frequency and voltage based on temperature.

---

## 🏗️ Architecture & Core Components

1.  **`main.cpp` (Orchestrator only — does NOT render)**
    *   Handles WiFi connection, mDNS resolution, OTA/Web UI server startup, and coordinates all subsystems.
    *   `loop()` runs on core 1: touch dispatch, screen switching, and calling into `UiRenderer` — it does not draw anything itself.
    *   `networkTask()` runs on core 0 (FreeRTOS, pinned): all HTTP(S) I/O, Telegram polling, the AI autopilot tick, OTA (`ArduinoOTA.handle()`), and the Web UI server (`webServer.handleClient()`).
    *   Manages `currentMode` (`AUTOPILOT` vs `MANUAL`) and a handful of `std::atomic`/spinlock-protected snapshot structs shared between the two cores — see `MainScreenSnapshot`/`dataMux` in `main.cpp`. Never add cross-core shared state without one of those two patterns.

2.  **`UiRenderer` (`core/`, all screen rendering)**
    *   Owns every pixel drawn on the CYD: Main (telemetry + temp/hashrate sparklines with "T"/"H" labels), Controls, Diagnostics, and the OTA progress screen.
    *   Pure and natively testable via the `IDisplay` mock — `main.cpp` only calls into it, never draws directly.
    *   Per-line "dirty check" caching avoids full-screen flicker: a line only repaints if its text/color actually changed since the last call.

3.  **`EspHttpClient` (HTTP Layer)**
    *   Custom wrapper around `HTTPClient`.
    *   *Crucial Detail:* Standard `HTTPClient` fails on chunked transfer encoding from DeepSeek. This wrapper explicitly handles headers and streams payload into `std::string` via a natively-tested `ChunkedDecoder` to avoid truncation. **Do not replace this with naive HTTPClient calls.**
    *   Root CAs for Telegram and DeepSeek are pinned (`RootCerts.h`) — never `setInsecure()` a known endpoint. A custom `AI_BASE_URL` can pin its own CA via `AI_ROOT_CA` in secrets.h; without it, a loud `log_w` fires once per boot.

4.  **`EspDisplay` (Hardware Abstraction - CYD)**
    *   Uses `TFT_eSPI` for drawing on the display.
    *   Uses `XPT2046_Touchscreen` for touch input.
    *   *Crucial Detail:* The CYD touch controller is on a **separate SPI bus** from the display. It must be initialized with the global `SPI.begin(25, 39, 32, 33)` before calling `ts.begin()`. Do NOT attempt to use `TFT_eSPI`'s internal touch functions.

5.  **`BitaxeController` (Telemetry)**
    *   Polls `http://<bitaxe_ip>/api/system/info` every 2 seconds; the IP is resolved via mDNS (`bitaxe.local`) on connect/reconnect, falling back to the static `BITAXE_IP` in secrets.h if that fails.
    *   Sets flags: `isOverheating` (>= 70.0°C) and `isTooCold` (0 < temp <= 60.0°C; a 0.0°C reading is treated as a sensor glitch).
    *   All thresholds and safe freq/volt ranges live in `include/core/Limits.h` — the single source of truth used by every code path that changes miner settings (AI autopilot, AI chat, `/set`, `/fan`, screen presets, benchmark — all seven paths, verified in the #75 security review).

6.  **`TelemetryHistory` (`core/`, 20-sample/10-minute ring buffer)**
    *   Backs both the `/status` trend line and the Main screen's sparklines (`copySparklineData`, oldest-first).

7.  **`DeepSeekOptimizer` (The AI Autopilot)**
    *   Only fires if Autopilot is ON, a temperature threshold is breached, and 30 seconds have passed since the last optimization (cooldown).
    *   Sends telemetry and a strict system prompt to `https://api.deepseek.com/chat/completions`.
    *   Requires output in strict JSON format: `{"reply": "...", "frequency": X, "coreVoltage": Y, "mode": "auto"}`.
    *   *Crucial Detail:* Parses JSON using `contentDoc["frequency"] | 0` fallback instead of strict type checking to avoid crashes if AI hallucinates string types.

8.  **`TelegramNotifier` (Remote Control)**
    *   Uses Telegram Bot API (long polling via `getUpdates?timeout=0`).
    *   Handles commands: `/status`, `/auto`, `/manual`, `/set`, `/fan`, `/restart`, `/history`, `/bench`, `/why`, `/esp`, `/help`.
    *   Registers a persistent bot menu via `/setMyCommands` on boot.
    *   Any natural language message is forwarded to `DeepSeekOptimizer::askQuestion`.
    *   Only obeys the configured chat id; messages from any other chat are acknowledged and dropped.

9.  **`StatusJsonBuilder` (`core/`) + the OTA/Web UI receivers (`main.cpp`)**
    *   `main.cpp` runs an `ArduinoOTA` receiver (flash over WiFi: `pio run -e esp32-cyd-ota -t upload`, password-gated via `OTA_PASSWORD` in secrets.h) and a `WebServer` on port 80 (`GET /` dashboard, `GET /api/status` JSON — read-only, LAN-only, no auth by design).
    *   `StatusJsonBuilder::build()` builds `/api/status`'s JSON from the exact same snapshot data `UiRenderer::renderTelemetry()` renders — don't add a second data path if extending the Web UI; read through the existing snapshot.
    *   OTA requires a specific partition table (`partitions_two_ota.csv`, two ~1.94MB slots, no SPIFFS) — see `platformio.ini`'s `board_build.partitions`. Changing partition layout again requires one serial reflash; see [README.md](README.md#ota-updates) for the full flashing guide including the one-time Windows Firewall rule.

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
    *   `pio` is not on the global PATH in this environment — it lives in the project's local `.venv`. Invoke it as `./.venv/Scripts/pio.exe` (Windows) rather than a bare `pio`, or the command will fail with "command not found".

6.  **Secrets**
    *   Credentials (WiFi, Telegram token, DeepSeek key, Bitaxe IP) live in `include/secrets.h`, which is gitignored. Copy `include/secrets.h.example` to get started. Never hardcode credentials in `main.cpp`.

7.  **Safety Validation**
    *   ANY parameter change (AI autopilot, AI chat, `/set` command) MUST pass `Limits::isValidSetting(freq, volt)` before being PATCHed to the Bitaxe. Never trust raw LLM output.
    *   The Telegram bot only obeys the chat id configured in `secrets.h` — messages from other chats are ignored.

8.  **Static analysis (`pio check`)**
    *   CI runs `pio check -e esp32-cyd --skip-packages --fail-on-defect=medium --fail-on-defect=high` and fails the build on medium+ findings in `src/`/`include/`. Scope and suppression rationale (`unusedFunction`, vendored `.pio/libdeps`) live as comments in `platformio.ini`.

9.  **Security posture**
    *   [SECURITY.md](SECURITY.md) documents the threat model (LAN attacker) and every accepted risk (unauthenticated read-only Web UI, ArduinoOTA's MD5 auth, no flash encryption, etc.). Read it before changing anything on a network-facing surface (Web UI, OTA, Telegram, HTTP client) — a change that looks like a security improvement might already be a documented, deliberate trade-off, and a change that looks harmless might invalidate one of the "hardened by design" properties listed there.

---

## 🚀 How to Continue
When prompted to add a feature, strictly adhere to the established architecture. If adding UI elements, add rendering to `UiRenderer` (not `main.cpp` directly) and update `EspDisplay` only for new primitive draw operations. If adding Bitaxe commands, use `BitaxeController`. If extending AI logic, carefully modify the DeepSeek prompt without breaking the JSON output format. If exposing new data externally (Web UI, Telegram), read it through the existing cross-core snapshot rather than adding a new path into `core/` state.

---

## 🔄 Workflow for Picking Up Backlog Work

**Step 0 — before touching any code:** run `gh issue list --state open` and present the list to the user as a table (number, title, a 1-2 sentence proposed implementation approach for each). Wait for the user to pick one issue — never start implementing without an explicit choice, and only work on one issue at a time.

Once an issue is chosen, follow this sequence for it — every step is mandatory, none are optional shortcuts:

1. **Branch**: `git checkout -b <type>/<issue-number>-<slug>` from `master`, where `<type>` is `feat`/`fix`/`docs`/`refactor`/`test`/`perf`/`ci` matching the change (this is the convention actually used across the repo's history, not a literal `feature/` prefix).
2. **TDD, always**: write a failing test first (confirm it's actually red — compile error or failing assertion), then implement until green. A PR for a functional change with no new/updated test is not acceptable.
3. **Run both suites**: `./.venv/Scripts/pio.exe test -e native` (all green) and `./.venv/Scripts/pio.exe run -e esp32-cyd` (record the resulting Flash/RAM % so headroom can be tracked over time — don't assume last session's numbers still apply). Use the local `.venv`'s `pio`, not a bare `pio` — see the PlatformIO Environment note above.
4. **Self-review — mandatory, before merge, regardless of CI status**: `git diff master...branch`, look for real bugs (not cosmetics), and report findings via the `ReportFindings` tool. CI passing is not a substitute for this step — CI does not catch logic bugs. If something is found, fix it in a new commit and document it in a PR comment, then re-review.
5. **Commit, push, open a PR** (`gh pr create`) with a description covering What / Why / How / Test results (native suite X/X, esp32-cyd build result + Flash/RAM %). No `Co-Authored-By` trailer — commits use the repo's configured git identity only.
6. **Post a self-review summary as a PR comment** (`gh pr comment`) — every PR, even when step 4 found nothing to fix. Don't skip this because CI is green; it's the record that a human-equivalent review actually happened, independent of CI's pass/fail.
7. **Wait for CI**: `gh pr checks <N>` — build-and-test (native + esp32-cyd), cppcheck static analysis, and gitleaks must all be green.
8. **Merge only after both green CI and the step-4 self-review are done**: `gh pr merge <N> --squash --delete-branch`.
9. `git checkout master && git pull`; delete any leftover local branch.
10. **Flash only after merge, never before.** Prefer OTA over WiFi (no cable, no COM-port juggling): `./.venv/Scripts/pio.exe run -e esp32-cyd-ota -t upload` (falls back to `--upload-port <device-IP>` if `axepilot.local` doesn't resolve; see [README.md](README.md#ota-updates) for the one-time Windows Firewall rule this needs). Serial (`-e esp32-cyd -t upload --upload-port COM4`) is the fallback, and the only option for a partition-table change.
11. For UI-facing changes, ask the user to confirm behavior/appearance on the physical device before closing the issue — this can't be verified from code alone.
12. **Close the issue** on GitHub with a comment summarizing what was done and linking the PR. If the PR body already said "Closes #N", GitHub auto-closes it on merge — still add the summary as a follow-up comment on the (already-closed) issue rather than skipping it.

**Escalate to the user instead of proceeding solo** when a change:
- alters the partition table (`partitions_two_ota.csv`) or other low-level flash layout — the current two-OTA-slot scheme was itself a deliberate architectural trade-off (see #10/#68); changing it again needs the same kind of discussion, plus a mandatory one-time serial reflash;
- is actually a credential/secret rotation (e.g. Telegram bot token, API keys) — that's a manual action in an external dashboard plus a `include/secrets.h` update, not something to automate;
- needs physical interaction with the device that a chat session can't perform (e.g. touchscreen calibration);
- touches a surface [SECURITY.md](SECURITY.md) documents as an accepted risk (e.g. adding auth to the Web UI, changing OTA's auth scheme) — confirm the user actually wants to revisit that trade-off rather than assuming the existing review missed something.
