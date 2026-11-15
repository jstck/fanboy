#include "schedule.h"

namespace fancore {

static bool crossesMidnight(const DayWindow& w) { return w.stopMin <= w.startMin; }

bool scheduleActive(const WeekSchedule& s, int weekday, int minuteOfDay) {
    if (weekday < 0 || weekday > 6 || minuteOfDay < 0 || minuteOfDay >= 24 * 60) return false;

    const DayWindow& today = s.days[weekday];
    if (today.enabled && minuteOfDay >= today.startMin) {
        if (crossesMidnight(today) || minuteOfDay < today.stopMin) return true;
    }

    // Tail of yesterday's window that ran past midnight.
    const DayWindow& yesterday = s.days[(weekday + 6) % 7];
    if (yesterday.enabled && crossesMidnight(yesterday) && minuteOfDay < yesterday.stopMin) return true;

    return false;
}

}  // namespace fancore
