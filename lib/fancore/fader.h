#pragma once

namespace fancore {

// Slew-rate limiter for the fan duty (in percent). fadeSec is the time for a
// full 0 -> 100 % sweep, so a smaller change takes proportionally less time.
// fadeSec <= 0 means instant.
//
// floor is the lowest running duty: starting from 0 jumps straight to it (or
// to the target, if lower), and fading toward 0 drops to 0 instead of going below it.
// Fans stall or don't start below their minimum duty, so a slow ramp through
// that range would only leave them stopped while powered.
class Fader {
public:
    float step(float target, float dt, float fadeSec, float floor = 0) {
        if (fadeSec <= 0) {
            value_ = target;
            return value_;
        }
        const float maxDelta = 100.0f * dt / fadeSec;
        if (target <= 0 && value_ - maxDelta < floor) {
            value_ = 0;
            return value_;
        }
        if (value_ <= 0 && target > 0 && floor > 0) {
            value_ = target < floor ? target : floor;
            return value_;
        }
        const float diff = target - value_;
        if (diff > maxDelta) value_ += maxDelta;
        else if (diff < -maxDelta) value_ -= maxDelta;
        else value_ = target;
        return value_;
    }

    float value() const { return value_; }
    void set(float v) { value_ = v; }

private:
    float value_ = 0;
};

}  // namespace fancore
