#include "mqtt.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"
#include "device.h"
#include "state.h"
#include "webserver.h"  // for broadcastState()

// Globals defined in main.cpp
extern bool   ledOn;
extern String ledColor;
extern bool   cycleMode;
extern void   applyLedState();

static WiFiClient   _wifiClient;
static PubSubClient mqttClient(_wifiClient);

static inline String mqttTopicState()   { return Device::id() + "/state"; }
static inline String mqttTopicCommand() { return Device::id() + "/command"; }

static void publishDiscovery() {
    JsonDocument doc;
    doc["name"]          = DEVICE_NAME;
    doc["unique_id"]     = Device::id();
    doc["state_topic"]   = mqttTopicState();
    doc["command_topic"] = mqttTopicCommand();
    doc["schema"]        = "json";

    JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
    modes.add("rgb");

    JsonObject device        = doc["device"].to<JsonObject>();
    device["identifiers"][0] = Device::id();
    device["name"]           = DEVICE_NAME;
    device["model"]          = "ESP32 Dev Board";
    device["manufacturer"]   = "MorrayGlow";

    String payload;
    serializeJson(doc, payload);

    String topic = String(MQTT_TOPIC_DISCOVERY_PREFIX) + "/light/" + Device::id() + "/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    bool   newOn    = ledOn;
    String newColor = ledColor;
    bool   newCycle = cycleMode;
    if (!parseMqttCommand(msg, newOn, newColor, newCycle)) return;

    ledOn     = newOn;
    ledColor  = newColor;
    cycleMode = newCycle;

    applyLedState();
    broadcastState();
    mqttClient.publish(mqttTopicState().c_str(), stateToJson(ledOn, ledColor, cycleMode).c_str(), true);
}

static void mqttReconnect() {
    if (mqttClient.connected()) return;
    String clientId   = Device::id() + "-" + String(random(0xffff), HEX);
    String stateTopic = mqttTopicState();
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                           stateTopic.c_str(), 0, true, "{\"on\":false}")) {
        mqttClient.subscribe(mqttTopicCommand().c_str());
        publishDiscovery();
        mqttClient.publish(stateTopic.c_str(), stateToJson(ledOn, ledColor, cycleMode).c_str(), true);
    }
}

void mqttSetup() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
}

void mqttLoop() {
    if (!mqttClient.connected()) {
        static unsigned long lastAttempt = 0;
        if (millis() - lastAttempt > 5000) {
            lastAttempt = millis();
            mqttReconnect();
        }
    }
    mqttClient.loop();
}

void mqttPublishState() {
    if (mqttClient.connected()) {
        mqttClient.publish(mqttTopicState().c_str(), stateToJson(ledOn, ledColor, cycleMode).c_str(), true);
    }
}
