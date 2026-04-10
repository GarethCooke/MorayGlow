# MorrayGlow

IoT LED strip controller — ESP32 firmware + web UI with Home Assistant integration.

## Project Summary

MorrayGlow controls an RGB LED strip via an **ESP32**. The device hosts a web UI and exposes a REST + WebSocket API. A browser-based client lets you toggle the strip on/off and pick a colour. The device also integrates with **Home Assistant** via MQTT auto-discovery, so it appears automatically as a light entity.

## Project Structure

```text
MorayGlow/
├── MorrayServer/               # ESP32 firmware (C++, PlatformIO)
│   ├── platformio.ini
│   ├── src/main.cpp            # WiFi, LittleFS, REST API, WebSocket
│   ├── include/
│   │   ├── config.h            # WiFi credentials, MQTT config — edit before flashing
│   │   ├── state.h             # Pure logic: stateToJson, rgbToHex, parseMqttCommand
│   │   └── mqtt.h              # MQTT client + Home Assistant discovery
│   └── test/
│       └── test_state.cpp      # Unity tests (run on host, not device)
└── MorrayClient/               # Web UI (plain HTML/CSS/JS — no framework)
    ├── index.html
    ├── css/style.css
    ├── js/
    │   ├── morray.js           # UMD utility module (buildWsUrl, isValidHexColor)
    │   └── app.js              # Main UI logic
    ├── test/
    │   └── morray.test.js      # Jest tests for morray.js
    └── test-server/            # Node.js mock server for UI development
        ├── server.js
        ├── logic.js            # Pure functions (testable without starting server)
        ├── db.json             # Persisted state between restarts
        └── test/
            └── server.test.js  # Jest tests for logic.js
```

## API

| Method | Endpoint       | Body                    | Description                                   |
| ------ | -------------- | ----------------------- | --------------------------------------------- |
| GET    | `/api/state`   | —                       | Returns `{"on": bool, "color": "#rrggbb"}`    |
| POST   | `/api/power`   | `{"on": bool}`          | Turn strip on or off                          |
| POST   | `/api/color`   | `{"color": "#rrggbb"}`  | Set strip colour                              |
| WS     | `/ws`          | —                       | Server pushes state on every change           |

## Design Decisions

| Decision       | Choice                                          | Reason                                                  |
| -------------- | ----------------------------------------------- | ------------------------------------------------------- |
| MCU            | ESP32 Dev Board                                 | Dual-core, Wi-Fi + Bluetooth built in                   |
| Build system   | PlatformIO                                      | Dependency management, board config, native test env    |
| Web server     | `esphome/ESPAsyncWebServer-esphome`             | Supports ESP32; async                                   |
| Filesystem     | LittleFS                                        | Serves client files from device flash                   |
| JSON           | ArduinoJson v7 (`JsonDocument`)                 | No capacity arg needed; widely supported                |
| MQTT           | `knolleary/PubSubClient` + `setBufferSize(512)` | Lightweight, well-maintained                            |
| HA integration | MQTT auto-discovery                             | Device appears automatically in Home Assistant          |
| Client         | Plain HTML/CSS/JS (no framework)                | Keeps LittleFS footprint small                          |
| Client module  | UMD pattern (`morray.js`)                       | Works as browser global and Node.js/Jest module         |
| Testability    | Pure functions in `state.h` / `logic.js`        | Compiled and tested on host without hardware            |
| C++ tests      | Unity (via PlatformIO native env)               | Bundled with PlatformIO; runs on host                   |
| JS tests       | Jest                                            | Standard, zero-config for CommonJS modules              |

## Before Flashing

Edit `MorrayServer/include/config.h` and set your WiFi SSID/password and MQTT broker IP before building and flashing.

## Running Tests

### MorrayServer — C++ firmware (Unity via PlatformIO)

Requires `gcc`/`g++` on your PATH (install MinGW-w64 if not present).

```powershell
cd MorrayServer
pio test -e native
```

### MorrayClient — browser JS logic (Jest)

```powershell
cd MorrayClient
npm install   # first time only
npm test
```

### Test server — Node.js logic (Jest)

```powershell
cd MorrayClient\test-server
npm install   # first time only
npm test
```

---

## Starting the Test Server

The test server lets you develop and test the web UI without the physical device.

```powershell
cd MorrayClient\test-server
npm install   # first time only
npm start
```

Then open [http://localhost:3000](http://localhost:3000) in a browser.

State is persisted to `MorrayClient\test-server\db.json` between restarts.
