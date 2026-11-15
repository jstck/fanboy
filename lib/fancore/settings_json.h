#pragma once
#include <ArduinoJson.h>

#include <string>

#include "settings.h"

namespace fancore {

void settingsToJson(const Settings& s, JsonObject out);

// Applies the fields present in `in` on top of `current` (partial updates are fine).
// On success writes the merged result to `out` and returns "". On failure returns an
// error message and leaves `out` untouched.
std::string settingsFromJson(JsonObjectConst in, const Settings& current, Settings& out);

// "HH:MM" <-> minutes after midnight. Returns -1 on parse failure.
int parseHhMm(const char* s);
std::string formatHhMm(int minutes);

}  // namespace fancore
