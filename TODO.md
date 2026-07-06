# 📋 AxePilot Roadmap

**The live roadmap is on GitHub Issues:** https://github.com/rizon0503/axepilot/issues

Organization:
- **[Milestones](https://github.com/rizon0503/axepilot/milestones):** UI & Touch · Reliability & Network · Tech Debt & Tests · AI & Insights
- Every feature is a separate issue; implementation lands via a Pull Request into the protected `master` (a green `build-and-test` check is required).

---

## 🏁 Archive of shipped work (2026-07-02 — 2026-07-05)

Development chronicle after the code review; details are in the git history.

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
