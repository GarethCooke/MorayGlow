#include "webserver.h"

#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiConfig.h>
#include <map>
#include <string>

#include "config.h"
#include "device.h"
#include "state.h"

// Globals defined in main.cpp
extern bool   ledOn;
extern String ledColor;
extern bool   cycleMode;
extern bool   apMode;
extern void   applyLedState();
extern void   mqttPublishState();

static AsyncWebServer _server(80);
static AsyncWebSocket _ws("/ws");

// ── Async WiFi scan cache ─────────────────────────────────────────────────────
// WiFi.scanNetworks(async=true) runs in the background; we poll it in
// webserverLoop() and cache the count so the request handler never blocks.

static int  _scanCount = WIFI_SCAN_FAILED;
static bool _scanReady = false;

static void pollScan() {
    int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return;
    if (result >= 0) {
        _scanCount = result;
        _scanReady = true;
    } else {
        // WIFI_SCAN_FAILED or not yet started — kick off a fresh scan
        _scanReady = false;
        WiFi.scanNetworks(/*async=*/true);
    }
}

void broadcastState() {
    _ws.textAll(stateToJson(ledOn, ledColor, cycleMode));
}

// ── Body handler helpers ──────────────────────────────────────────────────────

// Returns false (and sends a 400) when the body is still streaming or is not
// valid JSON. Pass doc by reference; it will be populated on success.
static bool parseJsonBody(uint8_t* data, size_t len, size_t index, size_t total,
                          JsonDocument& doc, AsyncWebServerRequest* req) {
    if (index + len < total) return false;
    if (deserializeJson(doc, (char*)data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return false;
    }
    return true;
}

// Broadcast state over WebSocket + MQTT, then reply to the REST caller.
static void broadcastAndRespond(AsyncWebServerRequest* req) {
    broadcastState();
    mqttPublishState();
    req->send(200, "application/json", stateToJson(ledOn, ledColor, cycleMode));
}

// Apply LED hardware state, then broadcast + respond.
static void applyAndRespond(AsyncWebServerRequest* req) {
    applyLedState();
    broadcastAndRespond(req);
}

// ── Body handlers ────────────────────────────────────────────────────────────

static void onBody_power(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    if (!doc["on"].is<bool>()) {
        req->send(400, "application/json", "{\"error\":\"on required\"}");
        return;
    }
    ledOn = doc["on"].as<bool>();
    applyAndRespond(req);
}

static void onBody_color(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    const char* color = doc["color"] | "";
    if (strlen(color) != 7 || color[0] != '#') {
        req->send(400, "application/json", "{\"error\":\"invalid color\"}");
        return;
    }
    ledColor  = color;
    cycleMode = false;  // picking a colour switches to static mode
    applyAndRespond(req);
}

static void onBody_mode(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                        size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    if (!doc["cycle"].is<bool>()) {
        req->send(400, "application/json", "{\"error\":\"cycle required\"}");
        return;
    }
    cycleMode = doc["cycle"].as<bool>();
    broadcastAndRespond(req);
}

static void onBody_networkset(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                              size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    const char* ssid = doc["ssid"] | "";
    const char* pwd  = doc["password"] | "";
    if (strlen(ssid) == 0) {
        req->send(400, "application/json", "{\"error\":\"ssid required\"}");
        return;
    }
    WiFiConfig cfg;
    cfg.setSSID(ssid);
    cfg.setPassword(pwd);
    EEPROM.put(0, cfg);
    EEPROM.commit();
    req->send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
}

// ── Setup / loop ─────────────────────────────────────────────────────────────

void webserverSetup() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    _ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
                   void*, uint8_t*, size_t) {
        if (type == WS_EVT_CONNECT) client->text(stateToJson(ledOn, ledColor, cycleMode));
    });
    _server.addHandler(&_ws);

    // Device info — available in both modes (used by setup.html before STA connection).
    _server.on("/api/deviceinfo", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["id"]  = Device::id();
        doc["url"] = Device::url();
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // Network setup endpoints — available in both modes so WiFi can be
    // reconfigured from the main UI without requiring a factory reset.
    _server.on("/api/networkdata", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!_scanReady) {
            // Scan still in progress — client should retry shortly
            req->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
            return;
        }
        // Deduplicate by SSID — 2.4 GHz and 5 GHz bands share a name;
        // keep the entry with the strongest signal.
        struct NetInfo { int rssi; bool secure; };
        std::map<std::string, NetInfo> best;
        for (int i = 0; i < _scanCount; i++) {
            std::string ssid   = WiFi.SSID(i).c_str();
            int         rssi   = WiFi.RSSI(i);
            bool        secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            auto it = best.find(ssid);
            if (it == best.end() || rssi > it->second.rssi)
                best[ssid] = {rssi, secure};
        }

        JsonDocument doc;
        doc["scanning"] = false;
        JsonArray arr   = doc["networks"].to<JsonArray>();
        for (auto& entry : best) {
            JsonObject net = arr.add<JsonObject>();
            net["ssid"]    = entry.first.c_str();
            net["rssi"]    = entry.second.rssi;
            net["secure"]  = entry.second.secure;
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
        // Free scan memory and start a fresh background scan for next request
        _scanReady = false;
        WiFi.scanDelete();
        WiFi.scanNetworks(/*async=*/true);
    });

    _server.on("/api/networkset", HTTP_POST,
               [](AsyncWebServerRequest* req) {}, nullptr, onBody_networkset);

    if (apMode) {
        // In AP mode redirect everything to the setup page
        _server.serveStatic("/", LittleFS, "/").setDefaultFile("setup.html");
        _server.onNotFound([](AsyncWebServerRequest* req) {
            if (req->url() == "/setup.html")
                req->send(503, "text/plain", "LittleFS not mounted — run uploadfs");
            else
                req->redirect("/setup.html");
        });
    } else {
        _server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
            req->send(200, "application/json", stateToJson(ledOn, ledColor, cycleMode));
        });

        // Device discovery — queries mDNS for all morrayglow devices (~2 s).
        _server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest* req) {
            req->send(200, "application/json", Device::queryDevicesJson());
        });

        _server.on("/api/power", HTTP_POST,
                   [](AsyncWebServerRequest* req) {}, nullptr, onBody_power);

        _server.on("/api/color", HTTP_POST,
                   [](AsyncWebServerRequest* req) {}, nullptr, onBody_color);

        _server.on("/api/mode", HTTP_POST,
                   [](AsyncWebServerRequest* req) {}, nullptr, onBody_mode);

        _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    }

    _server.begin();
    Serial.println("HTTP server started");

    // Start the first background scan immediately
    WiFi.scanNetworks(/*async=*/true);
}

void webserverLoop() {
    _ws.cleanupClients();
    pollScan();
}
