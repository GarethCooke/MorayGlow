#include "device.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>

String Device::_id;

void Device::init() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    _id = String("morrayglow-") + suffix;
}

const String& Device::id() {
    return _id;
}

String Device::apSsid() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String("MorrayGlow-") + suffix;
}

String Device::url() {
    return "http://" + _id + ".local";
}

String Device::queryDevicesJson() {
    JsonDocument doc;
    JsonArray    arr = doc["devices"].to<JsonArray>();

    {
        JsonObject self = arr.add<JsonObject>();
        self["id"]      = _id;
        self["ip"]      = WiFi.localIP().toString();
        self["url"]     = url();
    }

    int found = MDNS.queryService("morrayglow", "tcp");
    for (int i = 0; i < found; i++) {
        String peerId = MDNS.hostname(i);
        if (peerId.endsWith(".local")) peerId = peerId.substring(0, peerId.length() - 6);
        if (peerId == _id) continue;

        JsonObject peer = arr.add<JsonObject>();
        peer["id"]      = peerId;
        peer["ip"]      = MDNS.IP(i).toString();
        peer["url"]     = "http://" + peerId + ".local";  // peer — no Device::url() (different id)
    }

    String out;
    serializeJson(doc, out);
    return out;
}
