// MorayGlow — XIAO ESP32-S3 RGB LED Strip Controller
// Hardware: Seeed XIAO ESP32-S3, IRLML6344TR MOSFETs, MP1584 buck, 12V 5050 RGB strip
// See docs/Hardware/ for schematic, netlist, and BOM.
//
// PIN MAPPING
//   PIN_RED    GPIO1  D0  LEDC CH0  PWM 1kHz 8-bit
//   PIN_GREEN  GPIO2  D1  LEDC CH1  PWM 1kHz 8-bit
//   PIN_BLUE   GPIO6  D5  LEDC CH2  PWM 1kHz 8-bit  (GPIO3 is a strapping pin — avoid)
//   PIN_STATUS GPIO4  D3  Output    Status LED (via 330Ω to GND)
//   PIN_BUTTON GPIO5  D4  Input     INPUT_PULLUP · hold 5s = factory reset

#include <Arduino.h>
#include <ButtonStatus.h>
#include <TwoStateValue.h>

#include <EspDevice.h>
#include <EspProvision.h>

#include "config.h"
#include "mqtt.h"
#include "state.h"
#include "webserver.h"

#define PIN_RED    1
#define PIN_GREEN  2
#define PIN_BLUE   6
#define PIN_STATUS 4
#define PIN_BUTTON 5

#define LEDC_CH_RED     0
#define LEDC_CH_GREEN   1
#define LEDC_CH_BLUE    2
#define LEDC_FREQ_HZ    1000
#define LEDC_RESOLUTION 8

#define HOLD_DURATION_MS          5000
#define FLASH_COUNT               5
#define FLASH_ON_MS               150
#define FLASH_OFF_MS              150
#define COLOUR_CHANGE_INTERVAL_MS 3000

struct Colour { uint8_t r, g, b; };

const Colour colourSequence[] = {
    {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0},
    {0, 255, 255}, {255, 0, 255}, {255, 255, 255}, {255, 128, 0},
};
const uint8_t NUM_COLOURS   = sizeof(colourSequence) / sizeof(colourSequence[0]);
uint8_t       currentColour = 0;

// ── LED state — referenced as extern by webserver.cpp and mqtt.cpp ───────────
bool   ledOn     = false;
String ledColor  = "#ffffff";
bool   cycleMode = true;

ButtonStatus* pBtn          = nullptr;
uint32_t      lastColourChange = 0;

// ── Hardware helpers ──────────────────────────────────────────────────────────

static void blinkStatus(int count, uint32_t onMs, uint32_t offMs) {
    for (int i = 0; i < count; i++) {
        digitalWrite(PIN_STATUS, HIGH); delay(onMs);
        digitalWrite(PIN_STATUS, LOW);  delay(offMs);
    }
}

static void setupLedChannel(uint8_t channel, uint8_t pin) {
    ledcSetup(channel, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(pin, channel);
}

void setColour(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LEDC_CH_RED, r);
    ledcWrite(LEDC_CH_GREEN, g);
    ledcWrite(LEDC_CH_BLUE, b);
}

void applyLedState() {
    if (!ledOn) { setColour(0, 0, 0); return; }
    long c = strtol(ledColor.c_str() + 1, nullptr, 16);
    setColour((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

// ── Arduino lifecycle ─────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    pinMode(PIN_STATUS, OUTPUT);

    setupLedChannel(LEDC_CH_RED,   PIN_RED);
    setupLedChannel(LEDC_CH_GREEN, PIN_GREEN);
    setupLedChannel(LEDC_CH_BLUE,  PIN_BLUE);

    pBtn = new ButtonStatus(
        make_shared_ptr_lite<TwoStateValue>(new DigitalPinValue(PIN_BUTTON, false)));
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    Provision.onStation([](AsyncWebServer& srv) {
        webserverSetup(srv);
        String mqttUser = Provision.getMqttUser();
        String mqttPass = Provision.getMqttPass();
        Mqtt.setup(EspDevice::id().c_str(), mqttUser.c_str(), mqttPass.c_str());
    });

    EspProvisionConfig cfg;
    cfg.apPassword       = "morayglow";
    cfg.nvsNamespace     = "morayglow";
    cfg.otaPassword      = OTA_PASSWORD;
    cfg.friendlyHostname = "morayglow";
    cfg.serviceType      = "morayglow";
    Provision.begin("morayglow", "MorayGlow", cfg);

    blinkStatus(2, 200, 200);
    applyLedState();
    digitalWrite(PIN_STATUS, HIGH);
    lastColourChange = millis();
}

void loop() {
    Provision.loop();

    if (!Provision.isApMode()) {
        webserverLoop();
        Mqtt.loop();
    }

    uint32_t now = millis();

    // ── Hold-to-reset: blink faster as hold approaches 5 s ──────────────────
    static uint32_t btnDownStart = 0;
    if (pBtn->isOn()) {
        if (btnDownStart == 0) btnDownStart = now;
        uint32_t held      = now - btnDownStart;
        uint32_t blinkRate = map(constrain(held, 0, HOLD_DURATION_MS),
                                 0, HOLD_DURATION_MS, 800, 80);
        digitalWrite(PIN_STATUS, ((now / blinkRate) % 2) ? HIGH : LOW);
        if (held >= HOLD_DURATION_MS) {
            setColour(0, 0, 0);
            blinkStatus(FLASH_COUNT, FLASH_ON_MS, FLASH_OFF_MS);
            Provision.factoryReset();
        }
    } else {
        btnDownStart = 0;
        digitalWrite(PIN_STATUS, HIGH);
    }

    // ── Auto colour cycle every 3 s ──────────────────────────────────────────
    if (cycleMode && !pBtn->isOn() && (now - lastColourChange >= COLOUR_CHANGE_INTERVAL_MS)) {
        currentColour        = (currentColour + 1) % NUM_COLOURS;
        const Colour& c      = colourSequence[currentColour];
        ledOn                = true;
        ledColor             = rgbToHex(c.r, c.g, c.b);
        applyLedState();
        if (!Provision.isApMode()) {
            broadcastState();
            Mqtt.publishState();
        }
        lastColourChange = now;
    }
}
