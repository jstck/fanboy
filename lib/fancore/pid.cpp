#include "pid.h"

namespace fancore {

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float Pid::update(float measurement, float setpoint, float dt) {
    if (dt <= 0) dt = 0;
    const float error = measurement - setpoint;

    float deriv = 0;
    if (havePrev_ && dt > 0) deriv = (measurement - prevMeas_) / dt;
    prevMeas_ = measurement;
    havePrev_ = true;

    const float p = kp * error;
    const float d = kd * deriv;

    // Conditional integration: only integrate if it doesn't push further into saturation.
    const float candidateI = integral_ + ki * error * dt;
    const float unsat = p + candidateI + d;
    const bool pushingHigh = unsat > outMax && error > 0;
    const bool pushingLow = unsat < outMin && error < 0;
    if (!pushingHigh && !pushingLow) integral_ = candidateI;
    integral_ = clampf(integral_, outMin, outMax);

    return clampf(p + integral_ + d, outMin, outMax);
}

}  // namespace fancore
