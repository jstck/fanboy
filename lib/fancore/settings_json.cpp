#include "settings_json.h"

#include <stdio.h>

namespace fancore {

int parseHhMm(const char* s) {
    if (!s) return -1;
    int h = 0, m = 0;
    char extra;
    if (sscanf(s, "%d:%d%c", &h, &m, &extra) != 2) return -1;
    if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return h * 60 + m;
}

std::string formatHhMm(int minutes) {
    char buf[8];
    snprintf(buf, sizeof buf, "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return buf;
}

void settingsToJson(const Settings& s, JsonObject o) {
    o["setpoint"] = s.setpoint;
    o["direction"] = directionName(s.direction);
    o["kp"] = s.kp;
    o["ki"] = s.ki;
    o["kd"] = s.kd;
    o["minDuty"] = s.minDuty;
    o["maxDuty"] = s.maxDuty;
    o["fadeSec"] = s.fadeSec;
    o["allowStop"] = s.allowStop;
    o["fallbackDuty"] = s.fallbackDuty;
    o["fanType"] = fanTypeName(s.fanType);
    o["pwmInvert"] = s.pwmInvert;
    o["tachPpr"] = s.tachPpr;
    o["kickDuty"] = s.kickDuty;
    o["kickSec"] = s.kickSec;
    o["mode"] = modeName(s.mode);
    o["tz"] = s.tz.c_str();
    JsonArray days = o["schedule"].to<JsonArray>();
    for (const auto& d : s.schedule.days) {
        JsonObject j = days.add<JsonObject>();
        j["en"] = d.enabled;
        j["start"] = formatHhMm(d.startMin);
        j["stop"] = formatHhMm(d.stopMin);
    }
}

// Reads a numeric field if present. Returns false if present but not a number.
static bool readFloat(JsonObjectConst in, const char* key, float& dst) {
    JsonVariantConst v = in[key];
    if (v.isNull()) return true;
    if (!v.is<float>()) return false;
    dst = v.as<float>();
    return true;
}

std::string settingsFromJson(JsonObjectConst in, const Settings& current, Settings& out) {
    Settings s = current;

    struct {
        const char* key;
        float* dst;
    } floats[] = {
        {"setpoint", &s.setpoint}, {"kp", &s.kp},           {"ki", &s.ki},
        {"kd", &s.kd},             {"minDuty", &s.minDuty}, {"maxDuty", &s.maxDuty},
        {"fadeSec", &s.fadeSec},   {"fallbackDuty", &s.fallbackDuty}, {"kickDuty", &s.kickDuty},
        {"kickSec", &s.kickSec},
    };
    for (auto& f : floats)
        if (!readFloat(in, f.key, *f.dst)) return std::string(f.key) + " must be a number";

    if (!in["allowStop"].isNull()) {
        if (!in["allowStop"].is<bool>()) return "allowStop must be true/false";
        s.allowStop = in["allowStop"].as<bool>();
    }
    if (!in["pwmInvert"].isNull()) {
        if (!in["pwmInvert"].is<bool>()) return "pwmInvert must be true/false";
        s.pwmInvert = in["pwmInvert"].as<bool>();
    }
    if (!in["tachPpr"].isNull()) {
        if (!in["tachPpr"].is<int>()) return "tachPpr must be a whole number";
        s.tachPpr = in["tachPpr"].as<int>();
    }
    if (!in["direction"].isNull()) {
        if (!directionFromName(in["direction"].as<const char*>(), s.direction)) return "direction must be cool/heat";
    }
    if (!in["fanType"].isNull()) {
        if (!fanTypeFromName(in["fanType"].as<const char*>(), s.fanType)) return "fanType must be 4pin/3pin";
    }
    if (!in["mode"].isNull()) {
        if (!modeFromName(in["mode"].as<const char*>(), s.mode)) return "mode must be auto/on/off";
    }
    if (!in["tz"].isNull()) {
        if (!in["tz"].is<const char*>()) return "tz must be a string";
        s.tz = in["tz"].as<const char*>();
    }
    if (!in["schedule"].isNull()) {
        JsonArrayConst days = in["schedule"].as<JsonArrayConst>();
        if (days.isNull() || days.size() != 7) return "schedule must be an array of 7 days (Monday first)";
        for (size_t i = 0; i < 7; i++) {
            JsonObjectConst d = days[i];
            DayWindow& w = s.schedule.days[i];
            if (!d["en"].isNull()) w.enabled = d["en"].as<bool>();
            if (!d["start"].isNull()) {
                int m = parseHhMm(d["start"].as<const char*>());
                if (m < 0) return "schedule start must be HH:MM";
                w.startMin = m;
            }
            if (!d["stop"].isNull()) {
                int m = parseHhMm(d["stop"].as<const char*>());
                if (m < 0) return "schedule stop must be HH:MM";
                w.stopMin = m;
            }
        }
    }

    std::string err = validate(s);
    if (!err.empty()) return err;
    out = s;
    return "";
}

}  // namespace fancore
