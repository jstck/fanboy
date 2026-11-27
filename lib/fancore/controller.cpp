#include "controller.h"

namespace fancore {

const char* stateName(State s) {
    switch (s) {
        case State::Idle: return "idle";
        case State::FadingIn: return "fading_in";
        case State::Regulating: return "regulating";
        case State::Standby: return "standby";
        case State::FadingOut: return "fading_out";
        case State::Fault: return "fault";
    }
    return "idle";
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void Controller::resumeFrom(float duty) {
    fader_.set(clampf(duty, 0, 100));
    active_ = false;  // so the next update() re-initialises like a new activation
    fadingIn_ = false;
    stopped_ = false;
}

void Controller::update(const Settings& s, const ControlInput& in) {
    const float dt = in.dt > 0 ? in.dt : 0;
    pidDt_ += dt;

    if (in.newSample) {
        if (in.sampleValid) {
            badReads_ = 0;
            temp_ = in.temp;
            haveTemp_ = true;
        } else if (badReads_ < kFaultAfterBadReads) {
            badReads_++;
        }
    }
    const bool fault = sensorFault();

    const bool want = s.mode == Mode::On ||
                      (s.mode == Mode::Auto && in.timeValid &&
                       scheduleActive(s.schedule, in.weekday, in.minuteOfDay));

    if (want && !active_) {
        pid_.reset();
        raw_ = 0;
        pidDt_ = 0;
        fadingIn_ = true;
        stopped_ = false;
        belowTimer_ = 0;
    }
    active_ = want;

    pid_.kp = s.kp;
    pid_.ki = s.ki;
    pid_.kd = s.kd;
    pid_.outMin = 0;
    pid_.outMax = s.maxDuty;

    if (!active_) {
        target_ = 0;
        fadingIn_ = false;
    } else if (fault) {
        target_ = s.fallbackDuty;
        pid_.reset();
        pidDt_ = 0;
        stopped_ = false;
    } else if (!haveTemp_) {
        target_ = 0;  // waiting for the first reading after boot
    } else {
        if (in.newSample && in.sampleValid) {
            raw_ = pid_.update(temp_, s.setpoint, pidDt_);
            pidDt_ = 0;
        }

        if (s.allowStop) {
            if (stopped_) {
                if (raw_ >= s.minDuty) stopped_ = false;
            } else if (raw_ <= 0.01f) {
                belowTimer_ += dt;
                if (belowTimer_ >= kStopDelaySec) stopped_ = true;
            } else {
                belowTimer_ = 0;
            }
            if (stopped_) belowTimer_ = 0;
        } else {
            stopped_ = false;
            belowTimer_ = 0;
        }

        target_ = stopped_ ? 0 : clampf(raw_, s.minDuty, s.maxDuty);
    }

    const float d = fader_.step(target_, dt, s.fadeSec);
    if (fadingIn_ && d >= target_ - 0.01f && target_ > 0) fadingIn_ = false;

    if (!active_) state_ = d > 0 ? State::FadingOut : State::Idle;
    else if (fault) state_ = State::Fault;
    else if (fadingIn_) state_ = State::FadingIn;
    else if (stopped_) state_ = State::Standby;
    else state_ = State::Regulating;
}

}  // namespace fancore
