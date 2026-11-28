#pragma once
#include <string>

#include "schedule.h"

namespace fancore {

enum class Mode : unsigned char { Auto, On, Off };

const char* modeName(Mode m);
bool modeFromName(const char* s, Mode& out);

// Whether more fan moves the temperature down (fresh air in a warm room) or up
// (blowing warm air in).
enum class Direction : unsigned char { Cool, Heat };

const char* directionName(Direction d);
bool directionFromName(const char* s, Direction& out);

// FourPin: PWM on the fan's control pin plus a supply switch, which is cut when
// the fan should stop (4-pin fans keep spinning at 0 % PWM).
// ThreePin: 2/3-pin fans with the PWM chopping their supply. No switch, but a
// kickstart when starting from standstill, since they may not start at low duty.
enum class FanType : unsigned char { FourPin, ThreePin };

const char* fanTypeName(FanType t);
bool fanTypeFromName(const char* s, FanType& out);

struct Settings {
    float setpoint = 20.0f;  // °C
    Direction direction = Direction::Cool;

    // PI(D) gains. Output unit is duty %, input unit is °C, time in seconds.
    // kp: % per °C off target (above it when cooling, below when heating).
    // ki: % per (°C·s). kd: % per (°C/s).
    float kp = 30.0f;
    float ki = 0.05f;
    float kd = 0.0f;

    float minDuty = 25.0f;   // lowest running duty, %: the fans start and stop here rather than crawl below it
    float maxDuty = 100.0f;  // %
    float fadeSec = 30.0f;   // time for a full 0 -> 100 % sweep
    bool allowStop = false;  // stop the fans when they aren't needed (instead of holding minDuty)
    float fallbackDuty = 40.0f;  // used while active if the sensor fails

    // Hardware
    FanType fanType = FanType::FourPin;
    bool pwmInvert = true;     // PWM goes through an inverting (open-collector) driver
    int tachPpr = 2;           // tach pulses per fan revolution
    float kickDuty = 75.0f;    // ThreePin: duty % for the kickstart from standstill
    float kickSec = 0.5f;      // ThreePin: kickstart length, 0 = no kickstart

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
