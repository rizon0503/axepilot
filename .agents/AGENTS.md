# AxePilot - Project Rules & Memory

This file contains the persistent memory and strict architectural rules for the ESP32-CYD Bitaxe controller project. Always refer to these guidelines when making changes.

## 🎯 Architecture & Dependencies
1. **Platform**: PlatformIO (Environment: `esp32-cyd`).
2. **Hardware**: ESP32-2432S028R (Cheap Yellow Display).
3. **Libraries**:
   - `bodmer/TFT_eSPI` (Display)
   - `paulstoffregen/XPT2046_Touchscreen` (Resistive Touch)
   - `bblanchon/ArduinoJson` (v7)

## ⚠️ Critical Constraints & Nuances

### 1. HTTP Communication (`EspHttpClient`)
- Standard `HTTPClient` fails to properly read chunked transfer encoding from DeepSeek API because it blocks or returns empty bodies.
- **Rule**: ALWAYS use our custom `EspHttpClient` wrapper. It manually reads headers and streams the payload into `std::string` to guarantee full payload delivery without truncation.

### 2. Touch Screen SPI (`EspDisplay`)
- The CYD uses two **separate SPI buses**: one for the TFT display and one for the XPT2046 touch controller.
- **Rule**: Do NOT use `TFT_eSPI`'s built-in touch functions. The touch controller is initialized on a separate SPI bus using `SPI.begin(25, 39, 32, 33)` before calling `ts.begin()`. Modifying this will break touch input (Emergency Throttle).

### 3. Memory & Performance
- ESP32 heap can easily fragment if Arduino's `String` class is heavily used in loops.
- **Rule**: In `loop()` and UI rendering logic, STRICTLY use `snprintf` with a `char buf[256]` (or appropriate size) array for text formatting. Do NOT use `String` concatenation (`+`).
- **Rule**: Do NOT use `delay()` in the main loop. Use non-blocking timing with `sysTime.millis()`.

### 4. JSON Parsing (ArduinoJson v7)
- The project uses ArduinoJson v7.
- **Rule**: Avoid strict type checking like `.is<int>()` when parsing LLM outputs, as the LLM might return numbers as strings. Use the fallback operator instead (e.g., `int f = doc["frequency"] | 0;`).

### 5. Mode Switching & AI Integration (`DeepSeekOptimizer`)
- The system operates in `AUTOPILOT` or `MANUAL` mode.
- Autopilot automatically kicks in if `temperature >= 70.0` (throttle down) or `temperature <= 60.0` (speed up).
- If the AI is asked to change settings via Telegram chat, it outputs `"mode": "manual"` (or "auto") in its JSON.
- **Rule**: Ensure the DeepSeek system prompt strictly requests `"mode"` to allow the AI to switch states dynamically.

### 6. Telegram Bot (`TelegramNotifier`)
- The bot operates using non-blocking long-polling (`getUpdates?timeout=0`).
- Menu commands (`/status`, `/auto`, `/manual`, `/help`) are registered on boot via `/setMyCommands` to provide a persistent UI inside Telegram.
