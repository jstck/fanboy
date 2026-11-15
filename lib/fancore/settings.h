#pragma once
#include <string>

#include "schedule.h"

namespace fancore {

enum class Mode : unsigned char { Auto, On, Off };

const char* modeName(Mode m);
bool modeFromName(const char* s, Mode& out);

struct Settings {
    float setpoint = 20.0f;  // °C

    // PI(D) gains. Output unit is duty %, input unit is °C, time in seconds.
    // kp: % per °C above setpoint. ki: % per (°C·s). kd: % per (°C/s).
    float kp = 30.0f;
    float ki = 0.05f;
    float kd = 0.0f;

    float minDuty = 25.0f;   // stall floor of the fans, %
    float maxDuty = 100.0f;  // %
    float fadeSec = 30.0f;   // time for a full 0 -> 100 % sweep
    bool allowStop = false;  // stop the fans when no cooling is needed (instead of holding minDuty)
    float fallbackDuty = 40.0f;  // used while active if the sensor fails

    Mode mode = Mode::Auto;
    std::string tz = "UTC0";  // POSIX TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3"

    WeekSchedule schedule = defaultSchedule();

    static WeekSchedule defaultSchedule() {
        WeekSchedule s;
        for (auto& d : s.days) {
            d.enabled = true;
            d.startMin = 21 * 60;
            d.stopMin = 7 * 60;
        }
        return s;
    }
};

// Returns an empty string if valid, otherwise a description of the first problem.
std::string validate(const Settings& s);

}  // namespace fancore
