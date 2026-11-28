#pragma once

namespace fancore {

// Kickstart for 2/3-pin fans: when the output goes from 0 to running, hold it
// at kickDuty for kickSec so the fan gets going, then pass the duty through.
// A kick never lowers the duty, and stopping (0) cancels it at once. Call
// apply() every control tick with the time since the previous one.
class Kickstart {
public:
    float apply(float duty, float kickDuty, float kickSec, float dt) {
        if (duty <= 0) {
            running_ = kicking_ = false;
            return 0;
        }
        if (!running_) {
            running_ = true;
            elapsed_ = 0;
        } else {
            elapsed_ += dt;
        }
        kicking_ = elapsed_ < kickSec && kickDuty > duty;
        return kicking_ ? kickDuty : duty;
    }

    bool kicking() const { return kicking_; }

private:
    bool running_ = false;
    bool kicking_ = false;
    float elapsed_ = 0;
};

}  // namespace fancore
