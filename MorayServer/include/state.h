#pragma once

// Allow compiling on a native (non-Arduino) host for unit testing
#ifndef ARDUINO
#include <string>
#include <cstdio>
using String = std::string;
#endif

#include <ArduinoJson.h>

// Returns the full state as a JSON string: {"on":true,"color":"#rrggbb","cycle":true}
inline String stateToJson(bool on, const String& color, bool cycle) {
    JsonDocument doc;
    doc["on"]    = on;
    doc["color"] = color;
    doc["cycle"] = cycle;
    String out;
    serializeJson(doc, out);
    return out;
}

// Converts RGB components (0-255) to a "#rrggbb" hex string.
// Values are clamped to [0, 255].
inline String rgbToHex(int r, int g, int b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", r, g, b);
    return String(hex);
}

// Parses an MQTT command payload like {"state":"ON","color":{"r":255,"g":0,"b":0},"cycle":false}.
// Populates out* only when those keys are present.
// Returns false if the JSON is malformed; true otherwise.
inline bool parseMqttCommand(const String& payload, bool& outOn, String& outColor, bool& outCycle) {
    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;

    if (doc["state"].is<const char*>()) {
        outOn = (String(doc["state"].as<const char*>()) == "ON");
    }
    if (doc["color"]["r"].is<int>()) {
        outColor = rgbToHex(
            doc["color"]["r"].as<int>(),
            doc["color"]["g"].as<int>(),
            doc["color"]["b"].as<int>()
        );
    }
    if (doc["cycle"].is<bool>()) {
        outCycle = doc["cycle"].as<bool>();
    }
    return true;
}
