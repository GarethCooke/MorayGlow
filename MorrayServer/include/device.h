#pragma once

#include <Arduino.h>

class Device {
public:
    // Derives the device ID from the WiFi MAC address.
    // Must be called after WiFi.mode() so the MAC is stable.
    static void init();

    // Returns "morrayglow-a1b2c3" (last 3 MAC bytes, lowercase hex).
    static const String& id();

    // Returns the AP SSID: "MorrayGlow-A1B2C3" (uppercase suffix).
    static String apSsid();

    // Returns "http://morrayglow-a1b2c3.local"
    static String url();

    // Queries mDNS for all morrayglow devices and returns a JSON string
    // with a "devices" array (id, ip, url). Self is always first. Blocks ~2 s.
    static String queryDevicesJson();

private:
    static String _id;
};
