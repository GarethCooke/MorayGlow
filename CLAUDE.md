# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MorayGlow is an IoT LED strip controller. It has two separate sub-projects:

- **MorrayServer/** — ESP32-S3 firmware (C++, PlatformIO, Arduino framework) targeting the Seeed XIAO ESP32-S3
- **MorrayClient/** — Web UI served from the device (vanilla HTML/CSS/JS) + a Node.js dev mock server

## Build & Flash Commands (Firmware)

`pio` is not on PATH on Windows. Always invoke as:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" <args>
```

```bash
# Build firmware
pio run -e xiao_esp32s3

# Upload firmware to device (USB)
pio run -e xiao_esp32s3 -t upload

# Upload web UI files to LittleFS (USB)
pio run -e xiao_esp32s3 -t uploadfs

# Upload firmware over Wi-Fi (OTA)
pio run -e xiao_esp32s3_ota -t upload --upload-port 192.168.x.x

# Upload web UI files over Wi-Fi (OTA)
pio run -e xiao_esp32s3_ota -t uploadfs --upload-port 192.168.x.x

# Serial monitor
pio device monitor

# Run native C++ unit tests (no device needed)
pio test -e native
```

### OTA Notes

- OTA uses ArduinoOTA (password: `password`) via the `xiao_esp32s3_ota` PlatformIO environment.
- The device hostname is `morrayglow-XXXXXX.local` (MAC-based, printed to serial on boot).
- OTA requires the ESP32 to connect back to the host machine on an ephemeral port — Windows Firewall must allow inbound connections for `%USERPROFILE%\.platformio\penv\Scripts\python.exe`.
- IoT network isolation (common on mesh routers) will block OTA. Use USB or temporarily move the device to the main network for flashing.

## Client Dev Server

```bash
cd MorrayClient/test-server
npm install   # first time only
npm start     # → http://localhost:3000  (mirrors firmware API exactly)
```

Client JS tests (Jest):
```bash
cd MorrayClient
npm test
```

Test-server JS tests (Jest):
```bash
cd MorrayClient/test-server
npm test
```

## Architecture

### Data & Control Flow

```
Home Assistant
    ↕ MQTT (<device-id>/state, <device-id>/command)
ESP32-S3 Firmware (MorrayServer)
    ↕ REST + WebSocket (/api/state, /api/power, /api/color, /api/mode, /ws)
Web Browser (MorrayClient)
```

State changes from any source (REST, MQTT, WebSocket) call `applyLedState()` → `broadcastState()` → `mqttPublishState()`.

### Key Files

- **[include/state.h](MorrayServer/include/state.h)** — Shared state logic: `stateToJson(on, color, cycle)`, `rgbToHex`, `parseMqttCommand(payload, outOn, outColor, outCycle)`. Header-only with `inline` functions so it compiles on native (for unit tests). Uses `#ifndef ARDUINO` guards.
- **[include/config.h](MorrayServer/include/config.h)** — MQTT broker settings — **edit before flashing**.
- **[include/device.h](MorrayServer/include/device.h)** — `Device` static class: `init()`, `id()` (MAC-based `morrayglow-XXXXXX`), `apSsid()`, `queryDevicesJson()`.
- **[include/webserver.h](MorrayServer/include/webserver.h)** — Declarations: `broadcastState()`, `webserverSetup()`, `webserverLoop()`.
- **[include/mqtt.h](MorrayServer/include/mqtt.h)** — Declarations: `mqttSetup()`, `mqttLoop()`, `mqttPublishState()`.
- **[js/morray.js](MorrayClient/js/morray.js)** — UMD utility module (works in browser and Node.js): `buildWsUrl`, `isValidHexColor`.
- **[test-server/logic.js](MorrayClient/test-server/logic.js)** — Server-side validation logic mirroring `state.h` in JavaScript.

### Globals (main.cpp, extern'd into webserver.cpp and mqtt.cpp)

```cpp
bool   ledOn     = false;      // LED power state
String ledColor  = "#ffffff";  // current colour as hex
bool   cycleMode = true;       // true = auto cycle; false = hold static colour
bool   apMode    = false;      // true = captive portal / WiFi setup mode
```

### LED Hardware (main.cpp)

GPIO pin assignments on the XIAO ESP32-S3:

- `PIN_RED` (GPIO1), `PIN_GREEN` (GPIO2), `PIN_BLUE` (GPIO3) — PWM output via LEDC channels
- `PIN_STATUS` (GPIO4) — Status LED indicator
- `PIN_BUTTON` (GPIO5) — Button input with debounce and hold detection

`applyLedState()` drives the RGB strip via LEDC hardware PWM. Firmware features:

- **Cycle mode** (`cycleMode = true`): advances through 8 preset colours every 3 seconds
- **Static mode** (`cycleMode = false`): holds the user-selected colour
- **Mode toggle**: `POST /api/mode` with `{"cycle": bool}`; setting a colour via `/api/color` auto-switches to static
- **Button hold-to-reset**: hold 5 s to factory reset (status LED blinks faster as threshold approaches)
- **Boot blink**: 2-blink sequence on startup

### REST API

| Method | Path | Body | Response |
|--------|------|------|----------|
| GET | `/api/state` | — | `{"on":bool,"color":"#rrggbb","cycle":bool}` |
| POST | `/api/power` | `{"on":bool}` | state JSON |
| POST | `/api/color` | `{"color":"#rrggbb"}` | state JSON (also sets cycle=false) |
| POST | `/api/mode` | `{"cycle":bool}` | state JSON |
| GET | `/api/devices` | — | `{"devices":[{"id","ip","url"},...]}` |
| GET | `/api/deviceinfo` | — | `{"id","url"}` |
| GET | `/api/networkdata` | — | `{"scanning":bool,"networks":[{"ssid","rssi","secure"},...]}` |
| POST | `/api/networkset` | `{"ssid","password"}` | `{"ok":true}` then reboot |
| WS | `/ws` | — | server pushes state JSON on any change |

### MQTT Payload Format

MQTT commands use Home Assistant JSON schema, which **differs from the REST API**:

```json
// MQTT command (subscribe):  <device-id>/command
{ "state": "ON", "color": { "r": 255, "g": 0, "b": 0 }, "cycle": false }

// MQTT state (publish):      <device-id>/state
{ "on": true, "color": "#ff0000", "cycle": false }
```

HA auto-discovery is published to `homeassistant/light/<device-id>/config` on every MQTT reconnect.

Topics use the runtime device ID (e.g., `morrayglow-2cb120/state`), not a hardcoded name.

### Device Identity

- Device ID: `morrayglow-XXXXXX` where XXXXXX = last 3 bytes of MAC address (lowercase hex)
- AP SSID: `MorrayGlow-XXXXXX` (uppercase)
- mDNS hostname: `morrayglow-XXXXXX.local`
- `Device::init()` must be called after `WiFi.mode()` (MAC is stable from that point)

### WiFi / AP Mode

On first boot (no saved credentials) or after a factory reset, the device starts an access point:

- SSID: `MorrayGlow-XXXXXX`
- IP: `10.0.0.1`
- Captive portal serves `setup.html` — all requests redirect there
- After saving credentials the device restarts into station mode

### LittleFS

The web UI (`data/` directory) is uploaded to LittleFS via `uploadfs`. Files served:

- `index.html` — main control UI (power, mode toggle, colour picker, device list link)
- `setup.html` — WiFi setup captive portal (network scan, SSID/password entry)
- `devices.html` — multi-device discovery via mDNS
- `css/style.css`, `js/app.js`, `js/morray.js`

No build step — edit files in `data/` directly and re-run `uploadfs`.

## Library Conventions

- **ArduinoJson v7**: Use `JsonDocument` with no capacity argument (v7 is dynamic).
- **PubSubClient**: Always call `setBufferSize(512)` — default is too small for HA discovery payloads.
- **ESPAsyncWebServer**: POST body arrives in the `onBody` lambda (3rd arg to `server.on`), not the request handler.
- **WiFi scan**: Use async scan (`WiFi.scanNetworks(true)`) and poll via `WiFi.scanComplete()`. Never block in a handler.
