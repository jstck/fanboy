#pragma once

#include "fader.h"
#include "pid.h"
#include "settings.h"

namespace fancore {

enum class State : unsigned char {
    Idle,        // not active, fan off
    FadingIn,    // just became active, ramping up to the first target
    Regulating,  // active, PI controlling the duty
    Standby,     // active, but stopped because no cooling is needed (allowStop)
    FadingOut,   // no longer active, ramping down
    Fault,       // active, sensor broken -> fallback duty
};

const char* stateName(State s);

struct ControlInput {
    float dt = 0;              // seconds since previous update()
    bool newSample = false;    // a sensor read completed during this tick
    bool sampleValid = false;  // whether that read was valid (only meaningful if newSample)
    float temp = 0;            // °C (only meaningful if newSample && sampleValid)
    bool timeValid = false;    // wall clock synced (needed for the schedule)
    int weekday = 0;           // 0 = Monday
    int minuteOfDay = 0;
};

// Combines mode, schedule, PI, stop hysteresis, fade and sensor-fault handling
// into one fan duty. Call update() often (e.g. every 100-200 ms) so the fade is
// smooth; the PI only steps when a new sensor sample arrives.
class Controller {
public:
    static constexpr int kFaultAfterBadReads = 3;
    static constexpr float kStopDelaySec = 120;  // PI demand must be 0 this long before stopping

    void update(const Settings& s, const ControlInput& in);

    float duty() const { return fader_.value(); }  // % to write to the fan
    float target() const { return target_; }        // % the fader is heading to
    float pidOutput() const { return raw_; }
    State state() const { return state_; }
    bool active() const { return active_; }
    bool sensorFault() const { return badReads_ >= kFaultAfterBadReads; }
    bool haveTemp() const { return haveTemp_; }
    float temp() const { return temp_; }

private:
    Pid pid_;
    Fader fader_;
    State state_ = State::Idle;
    bool active_ = false;
    bool fadingIn_ = false;
    bool stopped_ = false;
    bool haveTemp_ = false;
    int badReads_ = 0;
    float temp_ = 0;
    float raw_ = 0;
    float target_ = 0;
    float pidDt_ = 0;
    float belowTimer_ = 0;
};

}  // namespace fancore
