#include <ArduinoJson.h>
#include <unity.h>

#include "controller.h"
#include "history.h"
#include "pid.h"
#include "schedule.h"
#include "settings_json.h"

using namespace fancore;

void setUp() {}
void tearDown() {}

// ---------- schedule ----------

static WeekSchedule emptySchedule() {
    WeekSchedule s;
    for (auto& d : s.days) d = DayWindow{};
    return s;
}

static const int MON = 0, TUE = 1, SUN = 6;
static int hm(int h, int m) { return h * 60 + m; }

void test_schedule_same_day_window() {
    WeekSchedule s = emptySchedule();
    s.days[MON] = {true, (uint16_t)hm(13, 0), (uint16_t)hm(15, 0)};
    TEST_ASSERT_FALSE(scheduleActive(s, MON, hm(12, 59)));
    TEST_ASSERT_TRUE(scheduleActive(s, MON, hm(13, 0)));
    TEST_ASSERT_TRUE(scheduleActive(s, MON, hm(14, 59)));
    TEST_ASSERT_FALSE(scheduleActive(s, MON, hm(15, 0)));
    TEST_ASSERT_FALSE(scheduleActive(s, TUE, hm(14, 0)));
}

void test_schedule_crosses_midnight() {
    WeekSchedule s = emptySchedule();
    s.days[MON] = {true, (uint16_t)hm(21, 30), (uint16_t)hm(7, 0)};
    TEST_ASSERT_FALSE(scheduleActive(s, MON, hm(6, 0)));  // belongs to (disabled) Sunday night
    TEST_ASSERT_FALSE(scheduleActive(s, MON, hm(21, 29)));
    TEST_ASSERT_TRUE(scheduleActive(s, MON, hm(21, 30)));
    TEST_ASSERT_TRUE(scheduleActive(s, MON, hm(23, 59)));
    TEST_ASSERT_TRUE(scheduleActive(s, TUE, hm(0, 0)));
    TEST_ASSERT_TRUE(scheduleActive(s, TUE, hm(6, 59)));
    TEST_ASSERT_FALSE(scheduleActive(s, TUE, hm(7, 0)));
    TEST_ASSERT_FALSE(scheduleActive(s, TUE, hm(22, 0)));  // Tuesday itself disabled
}

void test_schedule_sunday_wraps_to_monday() {
    WeekSchedule s = emptySchedule();
    s.days[SUN] = {true, (uint16_t)hm(22, 0), (uint16_t)hm(6, 0)};
    TEST_ASSERT_TRUE(scheduleActive(s, SUN, hm(23, 0)));
    TEST_ASSERT_TRUE(scheduleActive(s, MON, hm(5, 59)));
    TEST_ASSERT_FALSE(scheduleActive(s, MON, hm(6, 0)));
}

void test_schedule_disabled_day_tail_still_runs() {
    // Monday's window runs into Tuesday morning even if Tuesday itself is disabled,
    // and a disabled Monday does not produce a Tuesday-morning tail.
    WeekSchedule s = emptySchedule();
    s.days[MON] = {false, (uint16_t)hm(21, 0), (uint16_t)hm(7, 0)};
    s.days[TUE] = {true, (uint16_t)hm(21, 0), (uint16_t)hm(7, 0)};
    TEST_ASSERT_FALSE(scheduleActive(s, TUE, hm(3, 0)));
    TEST_ASSERT_TRUE(scheduleActive(s, TUE, hm(22, 0)));
}

void test_schedule_start_equals_stop_is_24h() {
    WeekSchedule s = emptySchedule();
    s.days[MON] = {true, (uint16_t)hm(12, 0), (uint16_t)hm(12, 0)};
    TEST_ASSERT_TRUE(scheduleActive(s, MON, hm(12, 0)));
    TEST_ASSERT_TRUE(scheduleActive(s, TUE, hm(11, 59)));
    TEST_ASSERT_FALSE(scheduleActive(s, TUE, hm(12, 0)));
}

void test_weekday_from_tm() {
    TEST_ASSERT_EQUAL(SUN, weekdayFromTm(0));
    TEST_ASSERT_EQUAL(MON, weekdayFromTm(1));
}

// ---------- PID ----------

void test_pid_proportional_direction_and_clamp() {
    Pid p;
    p.kp = 30;
    p.outMax = 80;
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, p.update(21, 20, 1));  // 1 °C too warm -> 30 %
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, p.update(19, 20, 1));   // too cold -> 0
    TEST_ASSERT_FLOAT_WITHIN(0.01, 80, p.update(30, 20, 1));  // clamped to max
}

void test_pid_anti_windup() {
    Pid p;
    p.kp = 10;
    p.ki = 1;
    p.outMax = 100;
    // Long time far too warm: output saturates, integral must not run away.
    for (int i = 0; i < 1000; i++) p.update(30, 20, 1);
    TEST_ASSERT_TRUE(p.integral() <= 100);
    // As soon as the room is below setpoint the output must drop quickly, not stay
    // pinned by a huge wound-up integral.
    float out = 0;
    for (int i = 0; i < 20; i++) out = p.update(19, 20, 1);
    TEST_ASSERT_TRUE(out < 100);
}

void test_pid_integral_does_not_go_negative() {
    Pid p;
    p.kp = 0;
    p.ki = 1;
    for (int i = 0; i < 100; i++) p.update(15, 20, 1);  // very cold for a long time
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0, p.integral());
    // Slightly warm: should respond immediately, not after unwinding a negative integral.
    TEST_ASSERT_TRUE(p.update(20.5, 20, 1) > 0);
}

// ---------- fader ----------

void test_fader_timing_and_reversal() {
    Fader f;
    // 30 s full scale -> 50 % takes 15 s.
    for (int i = 0; i < 14; i++) f.step(50, 1, 30);
    TEST_ASSERT_TRUE(f.value() < 50);
    f.step(50, 1, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 50, f.value());
    // Reverse mid-fade: goes down at the same rate from where it is.
    f.set(40);
    f.step(0, 3, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, f.value());
    f.step(100, 0, 0);  // zero fade time -> instant
    TEST_ASSERT_FLOAT_WITHIN(0.01, 100, f.value());
}

// ---------- controller ----------

static Settings testSettings() {
    Settings s;
    s.setpoint = 20;
    s.kp = 30;
    s.ki = 0;
    s.kd = 0;
    s.minDuty = 25;
    s.maxDuty = 100;
    s.fadeSec = 10;
    s.fallbackDuty = 40;
    s.mode = Mode::Auto;
    s.schedule = emptySchedule();
    s.schedule.days[MON] = {true, (uint16_t)hm(21, 0), (uint16_t)hm(7, 0)};
    return s;
}

static ControlInput tick(int minute, float temp, bool valid = true, int weekday = MON) {
    ControlInput in;
    in.dt = 1;
    in.newSample = true;
    in.sampleValid = valid;
    in.temp = temp;
    in.timeValid = true;
    in.weekday = weekday;
    in.minuteOfDay = minute;
    return in;
}

static void run(Controller& c, const Settings& s, int seconds, int minute, float temp, bool valid = true) {
    for (int i = 0; i < seconds; i++) c.update(s, tick(minute, temp, valid));
}

void test_controller_full_cycle() {
    Controller c;
    Settings s = testSettings();

    run(c, s, 5, hm(20, 0), 22);  // outside window
    TEST_ASSERT_EQUAL(State::Idle, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, c.duty());

    // Window starts: 2 °C over -> 60 % target, fade at 10 %/s.
    c.update(s, tick(hm(21, 0), 22));
    TEST_ASSERT_EQUAL(State::FadingIn, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 60, c.target());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 10, c.duty());
    run(c, s, 10, hm(21, 1), 22);
    TEST_ASSERT_EQUAL(State::Regulating, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 60, c.duty());

    // Cooler than setpoint -> held at min duty (allowStop off).
    run(c, s, 10, hm(22, 0), 19);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 25, c.duty());
    TEST_ASSERT_EQUAL(State::Regulating, c.state());

    // Window ends -> fade out to 0.
    c.update(s, tick(hm(7, 0), 19, true, TUE));
    TEST_ASSERT_EQUAL(State::FadingOut, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 15, c.duty());
    for (int i = 0; i < 5; i++) c.update(s, tick(hm(7, 0), 19, true, TUE));
    TEST_ASSERT_EQUAL(State::Idle, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, c.duty());
}

void test_controller_auto_needs_time_sync() {
    Controller c;
    Settings s = testSettings();
    ControlInput in = tick(hm(22, 0), 25);
    in.timeValid = false;
    for (int i = 0; i < 5; i++) c.update(s, in);
    TEST_ASSERT_FALSE(c.active());

    s.mode = Mode::On;  // manual on works without time
    c.update(s, in);
    TEST_ASSERT_TRUE(c.active());

    s.mode = Mode::Off;  // off overrides the schedule
    c.update(s, tick(hm(22, 0), 25));
    TEST_ASSERT_FALSE(c.active());
}

void test_controller_sensor_fault_fallback() {
    Controller c;
    Settings s = testSettings();
    s.fadeSec = 0;
    run(c, s, 2, hm(22, 0), 21);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, c.duty());

    run(c, s, 2, hm(22, 0), 0, false);  // two bad reads: not yet a fault
    TEST_ASSERT_FALSE(c.sensorFault());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, c.duty());

    c.update(s, tick(hm(22, 0), 0, false));  // third
    TEST_ASSERT_TRUE(c.sensorFault());
    TEST_ASSERT_EQUAL(State::Fault, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 40, c.duty());

    c.update(s, tick(hm(22, 0), 21));  // recovers on next good read
    TEST_ASSERT_FALSE(c.sensorFault());
    TEST_ASSERT_EQUAL(State::Regulating, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, c.duty());
}

void test_controller_allow_stop_hysteresis() {
    Controller c;
    Settings s = testSettings();
    s.fadeSec = 0;
    s.allowStop = true;
    run(c, s, 5, hm(22, 0), 21);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, c.duty());

    // Too cold: holds min duty until the stop delay elapses, then stops.
    run(c, s, (int)Controller::kStopDelaySec - 5, hm(22, 0), 19);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 25, c.duty());
    run(c, s, 10, hm(22, 0), 19);
    TEST_ASSERT_EQUAL(State::Standby, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0, c.duty());

    // Slightly warm (PI demand 15 % < minDuty 25 %): stays stopped.
    run(c, s, 5, hm(22, 0), 20.5);
    TEST_ASSERT_EQUAL(State::Standby, c.state());
    // Warm enough (demand 30 %): restarts.
    run(c, s, 1, hm(22, 0), 21);
    TEST_ASSERT_EQUAL(State::Regulating, c.state());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 30, c.duty());
}

// ---------- history ----------

void test_ring_overwrites_oldest() {
    Ring<int, 3> r;
    for (int i = 1; i <= 5; i++) r.push(i);
    TEST_ASSERT_EQUAL(3, r.size());
    TEST_ASSERT_EQUAL(3, r.at(0));
    TEST_ASSERT_EQUAL(5, r.at(2));
}

void test_history_averager() {
    HistoryAverager a;
    a.add(true, 20, 10, false, 1);
    a.add(true, 22, 30, true, 1);
    a.add(false, 0, 50, false, 2);  // invalid temp is ignored for the temp average
    HistorySample s = a.take(60);
    TEST_ASSERT_EQUAL(2100, s.tempCenti);
    TEST_ASSERT_EQUAL(35, s.duty);
    TEST_ASSERT_EQUAL(1, s.flags);
    HistorySample empty = a.take(120);
    TEST_ASSERT_EQUAL(HistorySample::kNoTemp, empty.tempCenti);
    TEST_ASSERT_EQUAL(0, empty.flags);
}

// ---------- settings JSON ----------

void test_settings_roundtrip() {
    Settings s;
    s.setpoint = 18.5f;
    s.mode = Mode::On;
    s.schedule.days[3] = {false, (uint16_t)hm(22, 15), (uint16_t)hm(6, 45)};
    JsonDocument doc;
    settingsToJson(s, doc.to<JsonObject>());
    Settings back;
    back.setpoint = 0;
    TEST_ASSERT_EQUAL_STRING("", settingsFromJson(doc.as<JsonObjectConst>(), Settings{}, back).c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 18.5, back.setpoint);
    TEST_ASSERT_EQUAL(Mode::On, back.mode);
    TEST_ASSERT_FALSE(back.schedule.days[3].enabled);
    TEST_ASSERT_EQUAL(hm(22, 15), back.schedule.days[3].startMin);
    TEST_ASSERT_EQUAL(hm(6, 45), back.schedule.days[3].stopMin);
}

static std::string applyJson(const char* json, Settings& out) {
    JsonDocument doc;
    deserializeJson(doc, json);
    return settingsFromJson(doc.as<JsonObjectConst>(), Settings{}, out);
}

void test_settings_partial_update_and_rejects() {
    Settings out;
    out.setpoint = 99;
    TEST_ASSERT_EQUAL_STRING("", applyJson("{\"setpoint\":22}", out).c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 22, out.setpoint);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 25, out.minDuty);  // untouched -> default

    Settings untouched;
    untouched.setpoint = 12;
    TEST_ASSERT_NOT_EQUAL(0, applyJson("{\"minDuty\":80,\"maxDuty\":50}", untouched).size());
    TEST_ASSERT_NOT_EQUAL(0, applyJson("{\"setpoint\":\"hot\"}", untouched).size());
    TEST_ASSERT_NOT_EQUAL(0, applyJson("{\"mode\":\"turbo\"}", untouched).size());
    TEST_ASSERT_NOT_EQUAL(0, applyJson("{\"schedule\":[{}]}", untouched).size());
    TEST_ASSERT_NOT_EQUAL(0, applyJson(
        "{\"schedule\":[{\"start\":\"25:00\"},{},{},{},{},{},{}]}", untouched).size());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 12, untouched.setpoint);  // failed updates don't write
}

void test_parse_hhmm() {
    TEST_ASSERT_EQUAL(hm(7, 5), parseHhMm("07:05"));
    TEST_ASSERT_EQUAL(-1, parseHhMm("24:00"));
    TEST_ASSERT_EQUAL(-1, parseHhMm("12:60"));
    TEST_ASSERT_EQUAL(-1, parseHhMm("12:00x"));
    TEST_ASSERT_EQUAL(-1, parseHhMm(nullptr));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_schedule_same_day_window);
    RUN_TEST(test_schedule_crosses_midnight);
    RUN_TEST(test_schedule_sunday_wraps_to_monday);
    RUN_TEST(test_schedule_disabled_day_tail_still_runs);
    RUN_TEST(test_schedule_start_equals_stop_is_24h);
    RUN_TEST(test_weekday_from_tm);
    RUN_TEST(test_pid_proportional_direction_and_clamp);
    RUN_TEST(test_pid_anti_windup);
    RUN_TEST(test_pid_integral_does_not_go_negative);
    RUN_TEST(test_fader_timing_and_reversal);
    RUN_TEST(test_controller_full_cycle);
    RUN_TEST(test_controller_auto_needs_time_sync);
    RUN_TEST(test_controller_sensor_fault_fallback);
    RUN_TEST(test_controller_allow_stop_hysteresis);
    RUN_TEST(test_ring_overwrites_oldest);
    RUN_TEST(test_history_averager);
    RUN_TEST(test_settings_roundtrip);
    RUN_TEST(test_settings_partial_update_and_rejects);
    RUN_TEST(test_parse_hhmm);
    return UNITY_END();
}
