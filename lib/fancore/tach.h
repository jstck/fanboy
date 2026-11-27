#pragma once
#include <stdint.h>

namespace fancore {

// Fan speed from tach pulses. Call update() about once a second with a
// snapshot of the ISR's pulse count and the micros() time of its last pulse.
// The frequency comes from the pulses between two updates divided by the time
// between the last pulse of each, which is far more precise than counting
// pulses per second. Unsigned arithmetic makes the micros() wrap harmless.
class TachMeter {
public:
    static constexpr uint32_t kStopMs = 2000;  // no pulse for this long -> stopped

    void update(uint32_t count, uint32_t lastPulseUs, uint32_t nowMs) {
        if (count != prevCount_) {
            lastPulseMs_ = nowMs;
            if (havePrev_ && lastPulseUs != prevUs_)
                hz_ = (float)(count - prevCount_) * 1e6f / (float)(uint32_t)(lastPulseUs - prevUs_);
            havePrev_ = true;
            prevCount_ = count;
            prevUs_ = lastPulseUs;
        } else if (nowMs - lastPulseMs_ > kStopMs) {
            // Start over, so the first reading after a restart isn't measured
            // from a pulse long ago.
            havePrev_ = false;
            hz_ = 0;
        }
    }

    float hz() const { return hz_; }
    float rpm(int pulsesPerRev) const { return pulsesPerRev > 0 ? hz_ * 60.0f / pulsesPerRev : 0; }

private:
    bool havePrev_ = false;
    uint32_t prevCount_ = 0;
    uint32_t prevUs_ = 0;
    uint32_t lastPulseMs_ = 0;
    float hz_ = 0;
};

}  // namespace fancore
