#pragma once

namespace fancore {

// PI(D) controller. error = measurement - setpoint, so a positive error
// (room warmer than wanted) gives positive output (more fan).
// Output is clamped to [outMin, outMax]. Anti-windup: the integral is clamped to
// the same range and is not integrated further while the output is saturated
// in the direction the error is pushing.
// The derivative acts on the measurement (not the error) to avoid setpoint kick.
class Pid {
public:
    float kp = 0, ki = 0, kd = 0;
    float outMin = 0, outMax = 100;

    void reset(float integral = 0) {
        integral_ = integral;
        havePrev_ = false;
    }

    float update(float measurement, float setpoint, float dt);

    float integral() const { return integral_; }

private:
    float integral_ = 0;
    float prevMeas_ = 0;
    bool havePrev_ = false;
};

}  // namespace fancore
