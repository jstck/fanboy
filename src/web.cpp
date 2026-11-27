#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <memory>

#include "app.h"
#include "settings_json.h"
#include "web_pages.h"  // generated from web/*.html by scripts/embed_web.py

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

void sendPage(AsyncWebServerRequest* req, const uint8_t* gz, size_t len) {
    AsyncWebServerResponse* res = req->beginResponse(200, "text/html", gz, len);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
}

void handleStatus(AsyncWebServerRequest* req) {
    app::Status st;
    app::TestMode test;
    Mode mode;
    float setpoint;
    {
        app::Guard g;
        st = app::status;
        test = app::test;
        mode = app::settings.mode;
        setpoint = app::settings.setpoint;
    }

    JsonDocument doc;
    if (st.haveTemp && !st.sensorFault) doc["temp"] = st.temp;
    else doc["temp"] = nullptr;
    doc["sensorFault"] = st.sensorFault;
    doc["sensorCount"] = st.sensorCount;
    doc["sensorId"] = st.sensorId;
    doc["duty"] = st.duty;
    doc["fanOn"] = st.fanOn;
    doc["pinDuty"] = st.pinDuty;
    doc["rpm"] = st.rpm;
    doc["tachHz"] = st.tachHz;
    doc["tachPulses"] = st.tachPulses;
    doc["target"] = st.target;
    doc["pid"] = st.pidOutput;
    doc["state"] = stateName(st.state);
    doc["active"] = st.active;
    doc["mode"] = modeName(mode);
    doc["setpoint"] = setpoint;
    // Manual test mode. duty/on are the requested values, which may be newer
    // than the output above (the control task applies them within 100 ms).
    JsonObject t = doc["test"].to<JsonObject>();
    const uint64_t now = app::nowMs();
    t["active"] = test.active(now);
    t["remainingMs"] = test.active(now) ? (uint32_t)(test.untilMs - now) : 0;
    t["duty"] = test.duty;
    t["on"] = test.on;
    t["leaseMs"] = TEST_LEASE_MS;
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

// POST /api/test
//   {"enable":true}   start test mode (from the current output), or renew it
//   {"enable":false}  stop now: back to automatic control, with a fade
//   {}                renew an active lease (the page's heartbeat)
//   "duty":0..100 and "on":bool set the manual output (test mode must be active)
// A request without "enable" while test mode isn't active gets 409, so a page
// whose lease ran out learns that instead of silently restarting test mode.
// Replies with the same JSON as /api/status.
void handlePostTest(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!json.is<JsonObject>()) return sendError(req, 400, "expected a JSON object");
    JsonObjectConst o = json.as<JsonObjectConst>();
    if (!o["enable"].isNull() && !o["enable"].is<bool>()) return sendError(req, 400, "enable must be true/false");
    if (!o["duty"].isNull() && (!o["duty"].is<float>() || o["duty"].as<float>() < 0 || o["duty"].as<float>() > 100))
        return sendError(req, 400, "duty must be 0..100");
    if (!o["on"].isNull() && !o["on"].is<bool>()) return sendError(req, 400, "on must be true/false");

    enum { Ok, NotActive } result = Ok;
    {
        app::Guard g;
        app::TestMode& t = app::test;
        const uint64_t now = app::nowMs();
        const bool wasActive = t.active(now);
        if (o["enable"].is<bool>() && !o["enable"].as<bool>()) {
            t.untilMs = 0;
        } else if (!wasActive && !o["enable"].is<bool>()) {
            result = NotActive;
        } else {
            if (!wasActive) {
                // Bumpless start: begin from what the fan is doing now.
                t.duty = app::status.duty;
                t.on = app::status.fanOn;
            }
            t.untilMs = now + TEST_LEASE_MS;
            if (o["duty"].is<float>()) t.duty = o["duty"].as<float>();
            if (o["on"].is<bool>()) t.on = o["on"].as<bool>();
        }
    }
    if (result == NotActive) return sendError(req, 409, "test mode is not active");
    handleStatus(req);
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
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) { sendPage(req, WEB_INDEX_GZ, WEB_INDEX_GZ_LEN); });
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest* req) { sendPage(req, WEB_TEST_GZ, WEB_TEST_GZ_LEN); });
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/history", HTTP_GET, handleHistory);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    auto* post = new AsyncCallbackJsonWebHandler("/api/settings", handlePostSettings);
    post->setMethod(HTTP_POST);
    server.addHandler(post);
    auto* test = new AsyncCallbackJsonWebHandler("/api/test", handlePostTest);
    test->setMethod(HTTP_POST);
    server.addHandler(test);
    server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "not found"); });
    server.begin();
}

}  // namespace app
