# Security

This document captures AxePilot's threat model and the security decisions behind it — both what is hardened and what is a consciously accepted risk. It exists so these decisions stay auditable and don't get silently violated by future changes. Produced by the security review in [#75](https://github.com/rizon0503/axepilot/issues/75).

## Threat model

AxePilot is a home-LAN device controlling a hobby ASIC miner. The adversary considered is:

- **An attacker on the same WiFi network** — the realistic case: a compromised laptop/phone/IoT device on the LAN.
- **The public-repo / CI dimension** — the source is public; nothing in the repo or its CI artifacts may leak credentials.

Out of scope: a physical attacker with the device in hand (see *Accepted risks*), and attacks on the WiFi network itself (WPA2 is treated as the outer security boundary).

## Hardened by design

These properties were verified in the review and must be preserved by future changes:

- **TLS root-CA pinning** ([RootCerts.h](include/hal/RootCerts.h)): api.telegram.org (GoDaddy G2) and api.deepseek.com (Amazon Root CA 1) are pinned, so a LAN MITM cannot impersonate the two cloud APIs that drive settings changes and bot commands. A custom `AI_BASE_URL` can be pinned via `AI_ROOT_CA` in secrets.h; without it the connection is *unverified and loudly logged* ([#76](https://github.com/rizon0503/axepilot/issues/76)).
- **A single settings gate**: every code path that changes miner frequency/voltage — AI autopilot, AI chat, Telegram `/set`, `/fan`, on-screen presets, benchmark — validates through `Limits::isValidSetting()` / the `Limits` ranges. A hostile or hallucinated AI response, or a prompt-injected chat message, can at worst pick a bad-but-safe value inside 400–625 MHz / 1000–1250 mV.
- **Response size gating**: every HTTP response body is rejected if it exceeds `Limits::MAX_JSON_RESPONSE_BYTES` (8 KB) *before* reaching `deserializeJson()`, bounding attacker-controlled allocations.
- **Telegram authorization**: the bot obeys exactly one chat id; messages from any other chat are acknowledged and dropped (`TelegramNotifier::pollNewMessage`, covered by tests).
- **Web UI is read-only by scope**: only `GET /` and `GET /api/status` are registered; there is deliberately no state-changing route. Adding one requires an authentication story first.
- **Secrets hygiene**: `include/secrets.h` is gitignored; CI and published release binaries build exclusively from the placeholder `secrets.h.example` ([#56](https://github.com/rizon0503/axepilot/issues/56)); a gitleaks scan runs on every PR.
- **OTA rollback safety**: the OTA slot switch is atomic (otadata); a failed or hostile upload leaves the running firmware untouched.

## Accepted risks

Documented trade-offs, judged proportionate for a home-LAN hobby device. Revisit if the deployment context changes (e.g. a shared/untrusted network).

| Risk | Rationale |
|---|---|
| **Web UI telemetry disclosure** — anyone on the LAN can read temperatures, hashrate, power, mode via `http://axepilot.local/` (no auth). | Read-only data of low sensitivity; pool credentials are *not* exposed via `/api/status`. Do **not** port-forward the device. |
| **ArduinoOTA MD5 challenge auth** — the OTA password handshake is MD5-based, adequate against casual actors, weak against a determined LAN attacker. | WPA2 is the outer boundary; flashing also requires knowing the password from gitignored secrets.h. The receiver is always on. |
| **Bitaxe AxeOS API is plain HTTP** — settings PATCH calls to the miner travel unencrypted on the LAN. | AxeOS design, not ours; anyone on the LAN can already reconfigure the miner directly, so AxePilot adds no new exposure. |
| **No flash encryption / secure boot** — a physical attacker can dump flash and extract every secret (WiFi, bot token, API keys, OTA password). | Consciously skipped: eFuse-based protection is irreversible, easy to brick, and disproportionate for a device in one's own home. Physical access is out of scope. |
| **Bot-token compromise = remote control** — whoever holds the Telegram bot token can send commands (within `Limits` bounds) and read telemetry. | Token lives only in gitignored secrets.h and on Telegram's side; history was scrubbed and re-verified with gitleaks. Rotate via @BotFather if leaked. |

## Reporting

This is a hobby project. If you find a security issue, please open a GitHub issue (or a private security advisory if it's sensitive).
