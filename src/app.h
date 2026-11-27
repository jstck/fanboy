#pragma once
// State shared between the control task, the web server and the main loop.
// Everything below is guarded by app::lock()/unlock() (use app::Guard).

#include <time.h>

#include "config.h"
#include "controller.h"
#include "history.h"
#include "settings.h"

namespace app {

using History = fancore::Ring<fancore::HistorySample, HISTORY_SAMPLES>;

struct Status {
    bool haveTemp = false;
    float temp = 0;
    bool sensorFault = false;
    int sensorCount = 0;
    char sensorId[24] = "";  // ROM ID of the sensor in use, hex bytes
    float duty = 0;          // % as seen by the fan
    bool fanOn = false;      // supply switch
    float pinDuty = 0;       // % on the PWM GPIO (after inversion)
    float rpm = 0;
    float tachHz = 0;
    uint32_t tachPulses = 0;  // since boot
    float target = 0;
    float pidOutput = 0;
    fancore::State state = fancore::State::Idle;
    bool active = false;
    bool testActive = false;
    uint32_t testRemainingMs = 0;
    bool timeValid = false;
    time_t now = 0;
    uint32_t uptimeSec = 0;
};

// Manual test mode: active while nowMs() < untilMs. The test page keeps
// renewing the lease; when it stops, control goes back to the controller.
struct TestMode {
    uint64_t untilMs = 0;
    float duty = 0;   // PWM duty % (as seen by the fan)
    bool on = false;  // supply switch
    bool active(uint64_t now) const { return now < untilMs; }
};

void initLock();
void lock();
void unlock();
struct Guard {
    Guard() { lock(); }
    ~Guard() { unlock(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};

extern fancore::Settings settings;
extern Status status;
extern History history;
extern TestMode test;

uint32_t uptimeSec();
uint64_t nowMs();  // monotonic, never wraps

// control.cpp
void startControlTask();
// settings_store.cpp
void loadSettings();                 // into app::settings (call before tasks start)
bool saveSettings(const fancore::Settings& s);
void applyTimezone(const char* tz);  // setenv TZ + tzset
// net.cpp
void netBegin();
void netLoop();
// web.cpp
void webBegin();

}  // namespace app
