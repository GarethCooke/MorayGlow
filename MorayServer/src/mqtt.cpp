#include "mqtt.h"
#include "config.h"
#include "state.h"
#include "webserver.h"
#include <ArduinoJson.h>
#include <EspDevice.h>

// State globals defined in main.cpp
extern bool   ledOn;
extern String ledColor;
extern bool   cycleMode;
extern void   applyLedState();

MqttLight Mqtt;

void MqttLight::setup(const char* deviceId) {
    _topicState     = String(deviceId) + "/state";
    _topicCommand   = String(deviceId) + "/command";
    _topicDiscovery = String(MQTT_TOPIC_DISCOVERY_PREFIX) + "/light/" + deviceId + "/config";

    EspMqttBase::Config cfg;
    cfg.host     = MQTT_HOST;
    cfg.port     = MQTT_PORT;
    cfg.user     = MQTT_USER;
    cfg.password = MQTT_PASSWORD;
    EspMqttBase::setup(cfg);
}

void MqttLight::publishDiscovery() {
    JsonDocument doc;
    doc["name"]          = DEVICE_NAME;
    doc["unique_id"]     = EspDevice::id();
    doc["state_topic"]   = _topicState;
    doc["command_topic"] = _topicCommand;
    doc["schema"]        = "json";

    JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
    modes.add("rgb");

    JsonObject device        = doc["device"].to<JsonObject>();
    device["identifiers"][0] = EspDevice::id();
    device["name"]           = DEVICE_NAME;
    device["model"]          = "ESP32 Dev Board";
    device["manufacturer"]   = "MorayGlow";

    String payload;
    serializeJson(doc, payload);
    publish(_topicDiscovery.c_str(), payload.c_str(), /*retain=*/true);
}

void MqttLight::onConnected() {
    subscribe(_topicCommand.c_str());
    publishDiscovery();
    publishState();
}

void MqttLight::onMessage(const char* topic, const char* payload) {
    String msg = payload;
    bool   newOn    = ledOn;
    String newColor = ledColor;
    bool   newCycle = cycleMode;
    if (!parseMqttCommand(msg, newOn, newColor, newCycle)) return;
    ledOn     = newOn;
    ledColor  = newColor;
    cycleMode = newCycle;
    applyLedState();
    broadcastState();
    publishState();
}

void MqttLight::publishState() {
    if (isConnected()) {
        publish(_topicState.c_str(), stateToJson(ledOn, ledColor, cycleMode).c_str(), /*retain=*/true);
    }
}
