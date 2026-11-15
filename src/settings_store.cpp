#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "app.h"
#include "settings_json.h"

namespace app {

static const char* kNamespace = "fanboy";
static const char* kKey = "cfg";

void applyTimezone(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

void loadSettings() {
    Preferences prefs;
    prefs.begin(kNamespace, true);
    String json = prefs.getString(kKey, "");
    prefs.end();

    fancore::Settings s;  // defaults
    if (json.length()) {
        JsonDocument doc;
        if (deserializeJson(doc, json) == DeserializationError::Ok) {
            std::string err = fancore::settingsFromJson(doc.as<JsonObjectConst>(), s, s);
            if (!err.empty()) Serial.printf("settings: stored settings invalid (%s), using defaults\n", err.c_str());
        } else {
            Serial.println("settings: stored JSON corrupt, using defaults");
        }
    } else {
        Serial.println("settings: none stored, using defaults");
    }

    Guard g;
    settings = s;
    applyTimezone(settings.tz.c_str());
}

bool saveSettings(const fancore::Settings& s) {
    JsonDocument doc;
    fancore::settingsToJson(s, doc.to<JsonObject>());
    String json;
    serializeJson(doc, json);
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return false;
    const bool ok = prefs.putString(kKey, json) == json.length();
    prefs.end();
    return ok;
}

}  // namespace app
