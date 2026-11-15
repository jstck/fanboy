#pragma once

namespace fancore {

// Slew-rate limiter for the fan duty (in percent). fadeSec is the time for a
// full 0 -> 100 % sweep, so a smaller change takes proportionally less time.
// fadeSec <= 0 means instant.
class Fader {
public:
    float step(float target, float dt, float fadeSec) {
        if (fadeSec <= 0) {
            value_ = target;
            return value_;
        }
        const float maxDelta = 100.0f * dt / fadeSec;
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
