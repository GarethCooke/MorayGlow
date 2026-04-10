#pragma once

// MQTT broker
#define MQTT_HOST     "192.168.1.x"
#define MQTT_PORT     1883
#define MQTT_USER     ""
#define MQTT_PASSWORD ""

// Device identity — AP SSID and MQTT topics are built at runtime from the MAC
// address (see device.h).  OTA_HOSTNAME uses the unique mDNS hostname.
#define DEVICE_NAME  "MorrayGlow"
#define OTA_HOSTNAME Device::id().c_str()

// MQTT auto-discovery prefix (Home Assistant default)
#define MQTT_TOPIC_DISCOVERY_PREFIX "homeassistant"
