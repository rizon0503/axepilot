# 📋 AxePilot Roadmap

**The live roadmap is on GitHub Issues:** https://github.com/rizon0503/axepilot/issues

Organization:
- **[Milestones](https://github.com/rizon0503/axepilot/milestones):** UI & Touch · Reliability & Network · Tech Debt & Tests · AI & Insights
- Every feature is a separate issue; implementation lands via a Pull Request into the protected `master` (a green `build-and-test` check is required).

---

## 🏁 Archive of shipped work (2026-07-06 — 2026-07-09)

Continues after the project-wide reset (squashed history, renamed to AxePilot, republished). Details are in the git history and closed PRs.

**Hardware UI:**
- CYD Controls screen (freq/volt presets, AUTO/MANUAL toggle, restart) + a Diagnostics screen (uptime, reset reason, WiFi RSSI, heap, interventions)
- Resistive touchscreen calibrated against measured XPT2046 corner readings; the emergency throttle only fires inside its calibrated button rect
- Screen-flicker fixes: per-line dirty-checking so only changed text/graphics repaint
- Temperature/hashrate sparklines on the Main screen, with "T"/"H" letter labels so the two graphs aren't distinguishable by color alone

**Reliability & networking:**
- Bitaxe discovery via mDNS (`bitaxe.local`) instead of only a hardcoded IP, with a static-IP fallback
- NTP time sync → real timestamps in `/history` and a fixed-UTC-hour daily digest (previously fired on any restart)
- Root CAs pinned for Telegram and DeepSeek instead of `setInsecure()`; a custom `AI_BASE_URL` can now pin its own CA too (`AI_ROOT_CA`), with a loud warning if it doesn't
- Structured `HttpResult` replaced a fragile `{error_...}` string convention; a natively-tested chunked-transfer decoder reads DeepSeek's streamed responses correctly
- JSON response size bounded before every `deserializeJson()` call
- Reboot-surviving counters in NVS (`/esp`), with a real bug fixed where the lifetime intervention count was overwriting instead of accumulating

**Architecture:**
- `UiRenderer` extracted from main.cpp — owns all screen rendering (Main/Controls/Diagnostics/OTA), independently testable via an `IDisplay` mock
- `AppState` extracted for screen-power and button-restore timers
- `TelemetryHistory` (20-sample/10-minute ring buffer) backs both the `/status` trend and the Main-screen sparklines
- `esp32-hal-log` levels replace stray `Serial.print` calls

**Infrastructure:**
- CI publishes a versioned GitHub Release (`vYYYY.MM.DD.<run_number>`) with the compiled `firmware.bin` on every push to `master` — built from placeholder secrets, a rollback reference point rather than a ready-to-flash binary
- OTA firmware updates over WiFi (`pio run -e esp32-cyd-ota -t upload`), via a custom two-slot partition table that also grew flash headroom
- Web UI (`http://axepilot.local/`) mirrors the Main screen's telemetry and sparklines, read-only, LAN-only
- Full implementation + security review: static analysis (`pio check`/cppcheck) wired into CI; [SECURITY.md](SECURITY.md) documents the threat model and accepted risks
- Project licensed under [PolyForm Strict 1.0.0](LICENSE.md) — source-available, noncommercial use only without permission
- Native tests: 66 → 182 cases

**Safety & hardware protection:**
- Git history scrubbed of secrets (`git filter-repo`); credentials live in the gitignored `include/secrets.h`
- Every freq/volt change validated through `Limits.h` (AI, /set, touchscreen button)
- The Telegram bot obeys only the configured chat id
- `SafetyGuard`: local emergency throttle at 72°C with no cloud dependency + fallback after 3 failed AI calls
- Watchdog on the network task (180 s, panic + reboot)

**Architecture:**
- Networking on a dedicated FreeRTOS task (core 0); the UI never blocks
- `CommandRouter` extracted from main.cpp; structured `AiChatResult` instead of magic substrings
- Single entry point `BitaxeController::applySettings(source)`
- TLS session reuse for Telegram (no handshake every 2 s)
- AUTO/MANUAL mode persisted in NVS

**Features:**
- Extended telemetry (power, fan, shares, bestDiff, uptime) + `TelemetryHistory` with a °C/min trend
- `InterventionLog` + `/history`, `BenchmarkRunner` + `/bench` (GH/W optimum)
- `/fan <25-100>|auto`, `/restart`, extended `/esp` (reset reason, min heap, WiFi drops)
- `ChatMemory`: AI chat conversation context (4 exchanges)
- Daily Telegram digest, configurable AI endpoint (`AI_BASE_URL`/`AI_MODEL`)

**Infrastructure:**
- Native tests: 15 → 66 cases (they used to be broken — they did not even compile)
- GitHub Actions CI: tests + firmware build on every push and PR
- Protected `master`: changes land only via PR with green CI
- README, secrets.h.example, published on GitHub
