#include "settings.h"

#include <string.h>

namespace fancore {

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Auto: return "auto";
        case Mode::On: return "on";
        case Mode::Off: return "off";
    }
    return "auto";
}

bool modeFromName(const char* s, Mode& out) {
    if (!s) return false;
    if (!strcmp(s, "auto")) out = Mode::Auto;
    else if (!strcmp(s, "on")) out = Mode::On;
    else if (!strcmp(s, "off")) out = Mode::Off;
    else return false;
    return true;
}

const char* directionName(Direction d) { return d == Direction::Heat ? "heat" : "cool"; }

bool directionFromName(const char* s, Direction& out) {
    if (!s) return false;
    if (!strcmp(s, "cool")) out = Direction::Cool;
    else if (!strcmp(s, "heat")) out = Direction::Heat;
    else return false;
    return true;
}

const char* fanTypeName(FanType t) { return t == FanType::ThreePin ? "3pin" : "4pin"; }

bool fanTypeFromName(const char* s, FanType& out) {
    if (!s) return false;
    if (!strcmp(s, "4pin")) out = FanType::FourPin;
    else if (!strcmp(s, "3pin")) out = FanType::ThreePin;
    else return false;
    return true;
}

static bool inRange(float v, float lo, float hi) { return v >= lo && v <= hi; }  // false for NaN

std::string validate(const Settings& s) {
    if (!inRange(s.setpoint, 5, 40)) return "setpoint must be 5..40";
    if (!inRange(s.kp, 0, 1000)) return "kp must be 0..1000";
    if (!inRange(s.ki, 0, 100)) return "ki must be 0..100";
    if (!inRange(s.kd, 0, 100000)) return "kd must be 0..100000";
    if (!inRange(s.minDuty, 0, 100)) return "minDuty must be 0..100";
    if (!inRange(s.maxDuty, 1, 100)) return "maxDuty must be 1..100";
    if (s.minDuty > s.maxDuty) return "minDuty must be <= maxDuty";
    if (!inRange(s.fadeSec, 0, 600)) return "fadeSec must be 0..600";
    if (!inRange(s.fallbackDuty, 0, 100)) return "fallbackDuty must be 0..100";
    if (s.tachPpr < 1 || s.tachPpr > 8) return "tachPpr must be 1..8";
    if (!inRange(s.kickDuty, 0, 100)) return "kickDuty must be 0..100";
    if (!inRange(s.kickSec, 0, 5)) return "kickSec must be 0..5";
    if (s.tz.empty() || s.tz.size() > 63) return "tz must be 1..63 chars";
    for (const auto& d : s.schedule.days)
        if (d.startMin >= 24 * 60 || d.stopMin >= 24 * 60) return "schedule times must be 00:00..23:59";
    return "";
}

}  // namespace fancore
