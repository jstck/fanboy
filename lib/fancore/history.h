#pragma once
#include <stddef.h>
#include <stdint.h>

namespace fancore {

// Fixed-capacity ring buffer; oldest entries are overwritten.
template <typename T, size_t N>
class Ring {
public:
    void push(const T& v) {
        buf_[head_] = v;
        head_ = (head_ + 1) % N;
        if (count_ < N) count_++;
    }
    size_t size() const { return count_; }
    static constexpr size_t capacity() { return N; }
    // i = 0 is the oldest entry.
    const T& at(size_t i) const { return buf_[(head_ + N - count_ + i) % N]; }
    void clear() { head_ = count_ = 0; }

private:
    T buf_[N];
    size_t head_ = 0;
    size_t count_ = 0;
};

struct HistorySample {
    uint32_t uptimeSec;  // when the sample was taken (device uptime)
    int16_t tempCenti;   // °C * 100, or kNoTemp
    uint8_t duty;        // average duty %, rounded
    uint8_t flags;       // bit0: active
    static constexpr int16_t kNoTemp = INT16_MIN;
};

// Accumulates per-tick values and averages them into one sample per interval.
class HistoryAverager {
public:
    void add(bool tempValid, float temp, float duty, bool active, float dt) {
        if (tempValid) {
            tempSum_ += temp * dt;
            tempW_ += dt;
        }
        dutySum_ += duty * dt;
        w_ += dt;
        if (active) active_ = true;
    }
    HistorySample take(uint32_t uptimeSec) {
        HistorySample s;
        s.uptimeSec = uptimeSec;
        s.tempCenti = tempW_ > 0 ? (int16_t)(tempSum_ / tempW_ * 100.0f + (tempSum_ >= 0 ? 0.5f : -0.5f))
                                 : HistorySample::kNoTemp;
        const float d = w_ > 0 ? dutySum_ / w_ : 0;
        s.duty = (uint8_t)(d + 0.5f);
        s.flags = active_ ? 1 : 0;
        tempSum_ = tempW_ = dutySum_ = w_ = 0;
        active_ = false;
        return s;
    }

private:
    float tempSum_ = 0, tempW_ = 0, dutySum_ = 0, w_ = 0;
    bool active_ = false;
};

}  // namespace fancore
