// Fanboy hardware test firmware: read the DS18B20, drive a 4-pin fan by hand.
// No regulation, no schedule. Web UI at http://HOSTNAME.local or the printed IP.
#include <Arduino.h>
#include <DallasTemperature.h>
#include <ESPmDNS.h>
#include <OneWire.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#if __has_include("hwtest_config.h")
#include "hwtest_config.h"
#else
#error "Copy hwtest/include/hwtest_config.example.h to hwtest/include/hwtest_config.h and edit it."
#endif

#include "page.h"

namespace {

// ---------------------------------------------------------------- state

float duty = DEFAULT_DUTY_PERCENT;  // % at the fan (what the fan sees)
bool fanOn = false;
bool pwmInverted = FAN_PWM_INVERTED;  // output stage inverts the GPIO signal
uint8_t ppr = 2;                      // tach pulses per revolution

bool haveTemp = false;
float temp = 0;
int sensorCount = 0;
bool haveRom = false;
uint8_t sensorRom[8];   // ROM ID of the sensor in use
char sensorId[24] = "";  // same, as hex bytes: "28 FF 64 1E 0F 16 03 2C"

float rpm = 0;
float tachHz = 0;

Preferences prefs;
WebServer server(80);

// ---------------------------------------------------------------- fan

// Duty on the GPIO itself, %.
float pinDuty() { return pwmInverted ? 100.0f - duty : duty; }

void fanWritePwm() {
    const uint32_t maxVal = 1u << FAN_PWM_BITS;  // LEDC treats 2^bits as 100 %
    uint32_t v = (uint32_t)(pinDuty() / 100.0f * maxVal + 0.5f);
    if (v > maxVal) v = maxVal;
    ledcWrite(PIN_FAN_PWM, v);
}

void fanWriteSwitch() {
    digitalWrite(PIN_FAN_SWITCH, (fanOn == (bool)FAN_SWITCH_ACTIVE_HIGH) ? HIGH : LOW);
}

// ---------------------------------------------------------------- tach

portMUX_TYPE tachMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t tachCount = 0;   // accepted edges since boot
volatile uint32_t tachLastUs = 0;  // micros() of the last accepted edge

void IRAM_ATTR onTach() {
    const uint32_t now = micros();
    portENTER_CRITICAL_ISR(&tachMux);
    if (now - tachLastUs >= TACH_MIN_INTERVAL_US) {
        tachLastUs = now;
        tachCount = tachCount + 1;
    }
    portEXIT_CRITICAL_ISR(&tachMux);
}

// Once a second: speed from the edge count and the time between the first and
// last edge of the window, which is far more precise than count-per-second.
void updateRpm() {
    static bool havePrev = false;
    static uint32_t prevCount = 0, prevUs = 0, lastPulseMs = 0;

    portENTER_CRITICAL(&tachMux);
    const uint32_t count = tachCount, lastUs = tachLastUs;
    portEXIT_CRITICAL(&tachMux);

    if (count != prevCount) {
        lastPulseMs = millis();
        if (havePrev && lastUs != prevUs) {
            tachHz = (count - prevCount) * 1e6f / (float)(lastUs - prevUs);
            rpm = tachHz * 60.0f / ppr;
        }
        havePrev = true;
        prevCount = count;
        prevUs = lastUs;
    } else if (millis() - lastPulseMs > 2000) {
        // Stopped. Start over so the first window after restarting isn't
        // measured from an old edge.
        havePrev = false;
        rpm = 0;
        tachHz = 0;
    }
}

// ---------------------------------------------------------------- DS18B20

OneWire oneWire(PIN_DS18B20);
DallasTemperature dallas(&oneWire);

void sensorScan() {
    dallas.begin();
    dallas.setWaitForConversion(false);
    dallas.setResolution(12);
    sensorCount = dallas.getDeviceCount();
    // With several sensors, use the one with the lowest ID (as displayed), so
    // the choice doesn't depend on bus search order.
    haveRom = false;
    for (int i = 0; i < sensorCount; i++) {
        DeviceAddress a;
        if (!dallas.getAddress(a, i)) continue;
        if (!haveRom || memcmp(a, sensorRom, 8) < 0) memcpy(sensorRom, a, 8);
        haveRom = true;
    }
    sensorId[0] = 0;
    if (haveRom) {
        for (int i = 0; i < 8; i++) snprintf(sensorId + 3 * i, 4, i < 7 ? "%02X " : "%02X", sensorRom[i]);
        dallas.setResolution(sensorRom, 12);
    }
}

// Non-blocking: request a conversion, read it back 800 ms later, every 2 s.
void sensorLoop() {
    static bool converting = false;
    static uint32_t requestMs = 0;
    const uint32_t now = millis();
    if (!converting) {
        if (now - requestMs < 2000) return;
        requestMs = now;
        if (!haveRom) sensorScan();  // missing or replugged
        if (!haveRom) {
            haveTemp = false;
            return;
        }
        dallas.requestTemperaturesByAddress(sensorRom);
        converting = true;
    } else if (now - requestMs >= 800) {
        converting = false;
        const float t = dallas.getTempC(sensorRom);
        // -127 = disconnected/CRC error; 85 = power-on value (no conversion happened).
        haveTemp = t != DEVICE_DISCONNECTED_C && t != 85.0f;
        if (haveTemp) temp = t;
        else haveRom = false;  // rescan next time
    }
}

// ---------------------------------------------------------------- web

void sendStatus() {
    portENTER_CRITICAL(&tachMux);
    const uint32_t pulses = tachCount;
    portEXIT_CRITICAL(&tachMux);

    char tempStr[16] = "null";
    if (haveTemp) snprintf(tempStr, sizeof tempStr, "%.2f", temp);

    char buf[512];
    snprintf(buf, sizeof buf,
             "{\"temp\":%s,\"sensors\":%d,\"sensorId\":\"%s\","
             "\"on\":%s,\"duty\":%.1f,\"pinDuty\":%.1f,\"inverted\":%s,\"rpm\":%.0f,\"tachHz\":%.2f,\"pulses\":%lu,\"ppr\":%u,"
             "\"ip\":\"%s\",\"ssid\":\"%s\",\"rssi\":%d,\"staticIp\":%s,\"uptime\":%lu,"
             "\"pins\":{\"ds18b20\":%d,\"pwm\":%d,\"tach\":%d,\"switch\":%d}}",
             tempStr, sensorCount, sensorId, fanOn ? "true" : "false", duty, pinDuty(), pwmInverted ? "true" : "false", rpm, tachHz,
             (unsigned long)pulses, ppr, WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(),
             WiFi.RSSI(), USE_STATIC_IP ? "true" : "false", (unsigned long)(millis() / 1000),
             PIN_DS18B20, PIN_FAN_PWM, PIN_FAN_TACH, PIN_FAN_SWITCH);
    server.send(200, "application/json", buf);
}

// POST form fields, all optional: on=0|1, duty=0..100, inv=0|1, ppr=1..8
void handleSet() {
    if (server.hasArg("on")) {
        fanOn = server.arg("on") == "1";
        fanWriteSwitch();
        Serial.printf("Fan switch %s\n", fanOn ? "ON" : "OFF");
    }
    if (server.hasArg("duty")) {
        duty = constrain(server.arg("duty").toFloat(), 0.0f, 100.0f);
        fanWritePwm();
    }
    if (server.hasArg("inv")) {
        pwmInverted = server.arg("inv") == "1";
        prefs.putBool("inv", pwmInverted);
        fanWritePwm();
    }
    if (server.hasArg("ppr")) {
        const long p = server.arg("ppr").toInt();
        if (p < 1 || p > 8) {
            server.send(400, "text/plain", "ppr must be 1..8");
            return;
        }
        ppr = (uint8_t)p;
        prefs.putUChar("ppr", ppr);
    }
    sendStatus();
}

void webBegin() {
    server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", PAGE_HTML); });
    server.on("/api/status", HTTP_GET, sendStatus);
    server.on("/api/set", HTTP_POST, handleSet);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();
}

// ---------------------------------------------------------------- WiFi

void wifiBegin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
#if USE_STATIC_IP
    IPAddress ip, gw, mask, dns1, dns2;
    if (ip.fromString(STATIC_IP) && gw.fromString(STATIC_GATEWAY) && mask.fromString(STATIC_SUBNET) &&
        dns1.fromString(STATIC_DNS1) && dns2.fromString(STATIC_DNS2)) {
        WiFi.config(ip, gw, mask, dns1, dns2);
        Serial.printf("WiFi: static IP %s\n", STATIC_IP);
    } else {
        Serial.println("WiFi: invalid static IP config, using DHCP");
    }
#else
    Serial.println("WiFi: DHCP");
#endif
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("WiFi: connecting to %s\n", WIFI_SSID);
}

void wifiLoop() {
    static bool wasConnected = false, mdnsStarted = false;
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected == wasConnected) return;
    wasConnected = connected;
    if (connected) {
        Serial.printf("WiFi: connected, IP %s, RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        if (!mdnsStarted && MDNS.begin(HOSTNAME)) {
            MDNS.addService("http", "tcp", 80);
            mdnsStarted = true;
        }
        Serial.printf("Web UI: http://%s/ or http://%s.local/\n", WiFi.localIP().toString().c_str(), HOSTNAME);
    } else {
        Serial.println("WiFi: disconnected");
    }
}

}  // namespace

void setup() {
    // Fan supply off first thing, before anything slow.
    pinMode(PIN_FAN_SWITCH, OUTPUT);
    fanWriteSwitch();

    Serial.begin(115200);
    delay(500);
    Serial.println("\nFanboy hardware test");
    Serial.printf("Pins: DS18B20=%d PWM=%d TACH=%d SWITCH=%d\n", PIN_DS18B20, PIN_FAN_PWM, PIN_FAN_TACH,
                  PIN_FAN_SWITCH);

    prefs.begin("hwtest", false);
    ppr = prefs.getUChar("ppr", 2);
    if (ppr < 1 || ppr > 8) ppr = 2;
    pwmInverted = prefs.getBool("inv", FAN_PWM_INVERTED);

    if (!ledcAttach(PIN_FAN_PWM, FAN_PWM_FREQ_HZ, FAN_PWM_BITS)) Serial.println("PWM: ledcAttach failed");
    fanWritePwm();

    pinMode(PIN_FAN_TACH, TACH_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), onTach, FALLING);

    sensorScan();
    Serial.printf("DS18B20: %d device(s)%s%s\n", sensorCount, haveRom ? ", using ID " : "", sensorId);

    wifiBegin();
    webBegin();
}

void loop() {
    server.handleClient();
    wifiLoop();
    sensorLoop();

    static uint32_t lastRpm = 0, lastPrint = 0;
    if (millis() - lastRpm >= 1000) {
        lastRpm = millis();
        updateRpm();
    }
    if (millis() - lastPrint >= 5000) {
        lastPrint = millis();
        Serial.printf("temp=%s%.2f  fan=%s duty=%.0f%% (pin %.0f%%)  rpm=%.0f\n", haveTemp ? "" : "(none) ",
                      temp, fanOn ? "on" : "off", duty, pinDuty(), rpm);
    }
    delay(2);
}
