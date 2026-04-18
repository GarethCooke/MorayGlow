#pragma once

// MQTT broker — edit IP before flashing; user/password set at runtime via /settings.html.
#define MQTT_HOST     "192.168.1.x"
#define MQTT_PORT     1883

// Device identity — AP SSID and MQTT topics are built at runtime from the MAC
// address (see device.h).  OTA_HOSTNAME uses the unique mDNS hostname.
#define DEVICE_NAME  "MorayGlow"

// OTA update password — default on first boot; override at runtime via /settings.html.
#define OTA_PASSWORD "password"

// MQTT auto-discovery prefix (Home Assistant default)
#define MQTT_TOPIC_DISCOVERY_PREFIX "homeassistant"
