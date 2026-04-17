#include "webserver.h"

#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <map>
#include <string>

#include "config.h"
#include "mqtt.h"
#include "state.h"

#include <EspDevice.h>
#include <EspProvision.h>

// State globals defined in main.cpp
extern bool   ledOn;
extern String ledColor;
extern bool   cycleMode;
extern void   applyLedState();

static AsyncWebSocket _ws("/ws");

// ── Async WiFi scan (station mode — for in-app network reconfiguration) ───────

static int  _scanCount = WIFI_SCAN_FAILED;
static bool _scanReady = false;

static void pollScan() {
    int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return;
    if (result >= 0) {
        _scanCount = result;
        _scanReady = true;
    } else {
        _scanReady = false;
        WiFi.scanNetworks(/*async=*/true);
    }
}

void broadcastState() {
    _ws.textAll(stateToJson(ledOn, ledColor, cycleMode));
}

// ── Body handler helpers ──────────────────────────────────────────────────────

static bool parseJsonBody(uint8_t* data, size_t len, size_t index, size_t total,
                          JsonDocument& doc, AsyncWebServerRequest* req) {
    if (index + len < total) return false;
    if (deserializeJson(doc, (char*)data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return false;
    }
    return true;
}

static void broadcastAndRespond(AsyncWebServerRequest* req) {
    broadcastState();
    Mqtt.publishState();
    req->send(200, "application/json", stateToJson(ledOn, ledColor, cycleMode));
}

static void applyAndRespond(AsyncWebServerRequest* req) {
    applyLedState();
    broadcastAndRespond(req);
}

// ── Body handlers ─────────────────────────────────────────────────────────────

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
    cycleMode = false;
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
    if (strlen(ssid) == 0) {
        req->send(400, "application/json", "{\"error\":\"ssid required\"}");
        return;
    }
    req->send(200, "application/json", "{\"ok\":true}");
    Provision.saveCredentialsAndRestart(ssid, doc["password"] | "");
}

// ── Setup / loop ──────────────────────────────────────────────────────────────

void webserverSetup(AsyncWebServer& server) {
    _ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
                   void*, uint8_t*, size_t) {
        if (type == WS_EVT_CONNECT) client->text(stateToJson(ledOn, ledColor, cycleMode));
    });
    server.addHandler(&_ws);

    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", stateToJson(ledOn, ledColor, cycleMode));
    });

    server.on("/api/deviceinfo", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["id"]   = EspDevice::id();
        doc["name"] = EspDevice::name();
        doc["url"]  = EspDevice::url();
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest* req) {
        // mDNS query for peer MorayGlow devices — blocks ~2 s
        JsonDocument doc;
        JsonArray arr = doc["devices"].to<JsonArray>();
        JsonObject self = arr.add<JsonObject>();
        self["id"]  = EspDevice::id();
        self["ip"]  = WiFi.localIP().toString();
        self["url"] = EspDevice::url();
        int found = MDNS.queryService("morayglow", "tcp");
        for (int i = 0; i < found; i++) {
            String peerId = MDNS.hostname(i);
            if (peerId.endsWith(".local")) peerId = peerId.substring(0, peerId.length() - 6);
            if (peerId == EspDevice::id()) continue;
            JsonObject peer = arr.add<JsonObject>();
            peer["id"]  = peerId;
            peer["ip"]  = MDNS.IP(i).toString();
            peer["url"] = "http://" + peerId + ".local";
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server.on("/api/networkdata", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!_scanReady) {
            req->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
            return;
        }
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
        _scanReady = false;
        WiFi.scanDelete();
        WiFi.scanNetworks(/*async=*/true);
    });

    server.on("/api/networkset", HTTP_POST,
              [](AsyncWebServerRequest* req) {}, nullptr, onBody_networkset);

    server.on("/api/power", HTTP_POST,
              [](AsyncWebServerRequest* req) {}, nullptr, onBody_power);

    server.on("/api/color", HTTP_POST,
              [](AsyncWebServerRequest* req) {}, nullptr, onBody_color);

    server.on("/api/mode", HTTP_POST,
              [](AsyncWebServerRequest* req) {}, nullptr, onBody_mode);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    WiFi.scanNetworks(/*async=*/true);
}

void webserverLoop() {
    _ws.cleanupClients();
    pollScan();
}
