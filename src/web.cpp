#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <memory>

#include "app.h"
#include "settings_json.h"
#include "web_index.h"  // generated from web/index.html by scripts/embed_web.py

using namespace fancore;

namespace {

AsyncWebServer server(80);

void sendJson(AsyncWebServerRequest* req, const JsonDocument& doc, int code = 200) {
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(code);
    res->addHeader("Cache-Control", "no-store");
    serializeJson(doc, *res);
    req->send(res);
}

void sendError(AsyncWebServerRequest* req, int code, const char* msg) {
    JsonDocument doc;
    doc["error"] = msg;
    sendJson(req, doc, code);
}

void handleIndex(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* res = req->beginResponse(200, "text/html", WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
}

void handleStatus(AsyncWebServerRequest* req) {
    app::Status st;
    Mode mode;
    float setpoint;
    {
        app::Guard g;
        st = app::status;
        mode = app::settings.mode;
        setpoint = app::settings.setpoint;
    }

    JsonDocument doc;
    if (st.haveTemp && !st.sensorFault) doc["temp"] = st.temp;
    else doc["temp"] = nullptr;
    doc["sensorFault"] = st.sensorFault;
    doc["duty"] = st.duty;
    doc["target"] = st.target;
    doc["pid"] = st.pidOutput;
    doc["state"] = stateName(st.state);
    doc["active"] = st.active;
    doc["mode"] = modeName(mode);
    doc["setpoint"] = setpoint;
    doc["timeValid"] = st.timeValid;
    if (st.timeValid) {
        char buf[32];
        struct tm lt;
        {
            app::Guard g;  // TZ may be changed concurrently
            localtime_r(&st.now, &lt);
        }
        strftime(buf, sizeof buf, "%a %Y-%m-%d %H:%M:%S", &lt);
        doc["time"] = buf;
    }
    doc["uptime"] = st.uptimeSec;
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["heap"] = ESP.getFreeHeap();
    sendJson(req, doc);
}

// {"now":<uptime>,"interval":60,"t":[uptime...],"temp":[°C|null...],"duty":[%...],"active":[0|1...]}
// Written by hand to avoid building a large JsonDocument.
void handleHistory(AsyncWebServerRequest* req) {
    std::unique_ptr<HistorySample[]> copy(new (std::nothrow) HistorySample[HISTORY_SAMPLES]);
    if (!copy) return sendError(req, 503, "out of memory");
    size_t n;
    uint32_t now;
    {
        app::Guard g;
        n = app::history.size();
        for (size_t i = 0; i < n; i++) copy[i] = app::history.at(i);
        now = app::status.uptimeSec;
    }

    AsyncResponseStream* res = req->beginResponseStream("application/json", 1024);
    res->addHeader("Cache-Control", "no-store");
    res->printf("{\"now\":%u,\"interval\":%d,\"t\":[", (unsigned)now, HISTORY_INTERVAL_SEC);
    for (size_t i = 0; i < n; i++) res->printf(i ? ",%u" : "%u", (unsigned)copy[i].uptimeSec);
    res->print("],\"temp\":[");
    for (size_t i = 0; i < n; i++) {
        if (i) res->print(',');
        if (copy[i].tempCenti == HistorySample::kNoTemp) res->print("null");
        else res->printf("%.2f", copy[i].tempCenti / 100.0f);
    }
    res->print("],\"duty\":[");
    for (size_t i = 0; i < n; i++) res->printf(i ? ",%u" : "%u", copy[i].duty);
    res->print("],\"active\":[");
    for (size_t i = 0; i < n; i++) res->print(i ? (copy[i].flags & 1 ? ",1" : ",0") : (copy[i].flags & 1 ? "1" : "0"));
    res->print("]}");
    req->send(res);
}

void handleGetSettings(AsyncWebServerRequest* req) {
    JsonDocument doc;
    {
        app::Guard g;
        settingsToJson(app::settings, doc.to<JsonObject>());
    }
    sendJson(req, doc);
}

void handlePostSettings(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!json.is<JsonObject>()) return sendError(req, 400, "expected a JSON object");

    Settings current, updated;
    {
        app::Guard g;
        current = app::settings;
    }
    std::string err = settingsFromJson(json.as<JsonObjectConst>(), current, updated);
    if (!err.empty()) return sendError(req, 400, err.c_str());

    if (!app::saveSettings(updated)) return sendError(req, 500, "failed to save settings");
    {
        app::Guard g;
        app::settings = updated;
        if (updated.tz != current.tz) app::applyTimezone(updated.tz.c_str());
    }
    Serial.println("settings: updated");

    JsonDocument doc;
    settingsToJson(updated, doc.to<JsonObject>());
    sendJson(req, doc);
}

}  // namespace

namespace app {

void webBegin() {
    server.on("/", HTTP_GET, handleIndex);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/history", HTTP_GET, handleHistory);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    auto* post = new AsyncCallbackJsonWebHandler("/api/settings", handlePostSettings);
    post->setMethod(HTTP_POST);
    server.addHandler(post);
    server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "not found"); });
    server.begin();
}

}  // namespace app
