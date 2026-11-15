#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "app.h"
#include "secrets.h"

namespace {

struct Network {
    const char* ssid;
    const char* pass;
};
const Network kNetworks[] = WIFI_NETWORKS;

WiFiMulti wifiMulti;
bool servicesStarted = false;
bool wasConnected = false;
uint32_t lastAttempt = 0;

void startServices() {
    // NTP: time zone comes from settings (already applied via TZ env).
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    {
        app::Guard g;
        app::applyTimezone(app::settings.tz.c_str());  // configTime resets TZ to UTC
    }

    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([] { Serial.println("OTA: start"); });
    ArduinoOTA.onEnd([] { Serial.println("OTA: done"); });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA: error %u\n", e); });
    ArduinoOTA.begin();  // also starts mDNS as HOSTNAME.local
    MDNS.addService("http", "tcp", 80);
    servicesStarted = true;
}

}  // namespace

namespace app {

void netBegin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.setAutoReconnect(true);
    for (const auto& n : kNetworks) wifiMulti.addAP(n.ssid, n.pass);
    Serial.printf("WiFi: %u network(s) configured\n", (unsigned)(sizeof kNetworks / sizeof kNetworks[0]));
}

void netLoop() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected && millis() - lastAttempt > 10000) {
        lastAttempt = millis();
        // Scans and connects to the strongest known network. Blocks for a few
        // seconds, which is fine: fan control runs in its own task.
        wifiMulti.run(8000);
    }
    if (connected != wasConnected) {
        wasConnected = connected;
        if (connected) {
            Serial.printf("WiFi: connected to %s, IP %s, RSSI %d\n", WiFi.SSID().c_str(),
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            if (!servicesStarted) startServices();
        } else {
            Serial.println("WiFi: disconnected");
        }
    }
    if (servicesStarted) ArduinoOTA.handle();
}

}  // namespace app
