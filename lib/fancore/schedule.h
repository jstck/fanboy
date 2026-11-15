#pragma once
#include <stdint.h>

namespace fancore {

// One regulating window per weekday. Times are minutes after midnight (0..1439).
// If stopMin <= startMin, the window runs past midnight and ends on the next day
// (e.g. Monday 21:30 -> 07:00 means Monday 21:30 until Tuesday 07:00).
// startMin == stopMin therefore means a full 24 h window.
struct DayWindow {
    bool enabled = false;
    uint16_t startMin = 0;
    uint16_t stopMin = 0;
};

// days[0] = Monday ... days[6] = Sunday
struct WeekSchedule {
    DayWindow days[7];
};

// weekday: 0 = Monday ... 6 = Sunday. minuteOfDay: 0..1439.
bool scheduleActive(const WeekSchedule& s, int weekday, int minuteOfDay);

// Convert tm_wday (0 = Sunday) to our Monday-first index.
inline int weekdayFromTm(int tmWday) { return (tmWday + 6) % 7; }

}  // namespace fancore
