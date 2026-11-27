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
    bool timeValid = false;
    time_t now = 0;
    uint32_t uptimeSec = 0;
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

uint32_t uptimeSec();

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
