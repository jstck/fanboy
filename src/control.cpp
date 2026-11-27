// Control task: sensor -> controller -> fan outputs (PWM + supply switch),
// plus tach measurement and history recording.
#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <string.h>

#include "app.h"
#include "tach.h"

using namespace fancore;

namespace {

// Non-blocking DS18B20 reader: requests a conversion, reads it back later.
// With several sensors on the bus it uses the one with the lowest ROM ID.
class TempSensor {
public:
    explicit TempSensor(uint8_t pin) : wire_(pin), dallas_(&wire_) {}

    void begin() {
        scan();
        Serial.printf("DS18B20: %d device(s)%s%s\n", count_, haveRom_ ? ", using ID " : "", id_);
    }

    // Returns true when a sample (valid or not) completed during this call.
    bool poll(uint32_t nowMs, float& temp, bool& valid) {
        if (!converting_) {
            if (nowMs - lastRequest_ < SENSOR_PERIOD_MS) return false;
            lastRequest_ = nowMs;
            if (!haveRom_) {
                scan();  // sensor missing or replugged
                if (!haveRom_) {
                    valid = false;
                    return true;
                }
            }
            dallas_.requestTemperaturesByAddress(rom_);
            converting_ = true;
            return false;
        }
        if (nowMs - lastRequest_ < SENSOR_CONVERSION_MS) return false;
        converting_ = false;
        const float t = dallas_.getTempC(rom_);
        // -127 = disconnected/CRC error; 85 = power-on reset value (conversion didn't happen).
        valid = t != DEVICE_DISCONNECTED_C && t != 85.0f && t > -40 && t < 80;
        temp = t;
        if (t == DEVICE_DISCONNECTED_C) haveRom_ = false;  // rescan next time
        return true;
    }

    int count() const { return count_; }
    const char* id() const { return id_; }  // "28 FF 64 1E 0F 16 03 2C", or "" if none

private:
    void scan() {
        dallas_.begin();
        dallas_.setWaitForConversion(false);
        count_ = dallas_.getDeviceCount();
        haveRom_ = false;
        for (int i = 0; i < count_; i++) {
            DeviceAddress a;
            if (!dallas_.getAddress(a, i)) continue;
            if (!haveRom_ || memcmp(a, rom_, 8) < 0) memcpy(rom_, a, 8);
            haveRom_ = true;
        }
        id_[0] = 0;
        if (haveRom_) {
            for (int i = 0; i < 8; i++) snprintf(id_ + 3 * i, 4, i < 7 ? "%02X " : "%02X", rom_[i]);
            dallas_.setResolution(rom_, 12);
        }
    }

    OneWire wire_;
    DallasTemperature dallas_;
    bool converting_ = false;
    uint32_t lastRequest_ = 0;
    int count_ = 0;
    bool haveRom_ = false;
    DeviceAddress rom_;
    char id_[24] = "";
};

// ---- tach ----

portMUX_TYPE tachMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t tachCount = 0;   // accepted pulses since boot
volatile uint32_t tachLastUs = 0;  // micros() of the last accepted pulse

void IRAM_ATTR onTach() {
    const uint32_t now = micros();
    portENTER_CRITICAL_ISR(&tachMux);
    if (now - tachLastUs >= TACH_MIN_INTERVAL_US) {
        tachLastUs = now;
        tachCount = tachCount + 1;
    }
    portEXIT_CRITICAL_ISR(&tachMux);
}

// ---- fan outputs ----

void fanBegin() {
    if (!ledcAttach(PIN_FAN_PWM, FAN_PWM_FREQ_HZ, FAN_PWM_BITS)) Serial.println("PWM: ledcAttach failed");
    pinMode(PIN_FAN_SWITCH, OUTPUT);
    pinMode(PIN_FAN_TACH, INPUT_PULLUP);  // external pull-up to 3.3V too
    attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), onTach, FALLING);
}

// duty: % as seen by the fan. Returns the duty on the GPIO itself.
float fanApply(float duty, bool on, bool invert) {
    const float pin = invert ? 100.0f - duty : duty;
    const uint32_t maxVal = 1u << FAN_PWM_BITS;  // LEDC treats 2^bits as 100 %
    uint32_t v = pin <= 0 ? 0 : (uint32_t)(pin / 100.0f * maxVal + 0.5f);
    if (v > maxVal) v = maxVal;
    ledcWrite(PIN_FAN_PWM, v);
    digitalWrite(PIN_FAN_SWITCH, on == (bool)FAN_SWITCH_ACTIVE_HIGH ? HIGH : LOW);
    return pin;
}

void controlTask(void*) {
    TempSensor sensor(PIN_DS18B20);
    sensor.begin();
    fanBegin();
    {
        app::Guard g;
        fanApply(0, false, app::settings.pwmInvert);
    }

    Controller controller;
    HistoryAverager avg;
    TachMeter tach;
    uint32_t last = millis(), lastTach = last;
    uint32_t nextHistory = app::uptimeSec() + HISTORY_INTERVAL_SEC;
    bool wasTesting = false;
    float testOutput = 0;  // effective duty during test mode (0 with the switch off)
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CONTROL_TICK_MS));
        const uint32_t nowMs = millis();
        const float dt = (nowMs - last) / 1000.0f;
        last = nowMs;

        ControlInput in;
        in.dt = dt;
        float t = 0;
        bool valid = false;
        in.newSample = sensor.poll(nowMs, t, valid);
        in.sampleValid = valid;
        in.temp = t;
        if (in.newSample && !valid) Serial.printf("DS18B20: invalid reading (%.2f)\n", t);

        portENTER_CRITICAL(&tachMux);
        const uint32_t pulses = tachCount, pulseUs = tachLastUs;
        portEXIT_CRITICAL(&tachMux);
        if (nowMs - lastTach >= 1000) {
            lastTach = nowMs;
            tach.update(pulses, pulseUs, nowMs);
        }

        const uint32_t up = app::uptimeSec();
        float duty;
        bool fanOn, invert;
        {
            app::Guard g;
            const time_t now = time(nullptr);
            in.timeValid = now > 1700000000;  // NTP has synced
            if (in.timeValid) {
                struct tm lt;
                localtime_r(&now, &lt);
                in.weekday = weekdayFromTm(lt.tm_wday);
                in.minuteOfDay = lt.tm_hour * 60 + lt.tm_min;
            }

            const uint64_t nowMs64 = app::nowMs();
            const bool testing = app::test.active(nowMs64);
            if (testing != wasTesting) {
                wasTesting = testing;
                Serial.println(testing ? "test mode: on (manual control)" : "test mode: off, back to automatic");
                // Fade from wherever test mode left the fan.
                if (!testing) controller.resumeFrom(testOutput);
            }

            // The controller keeps running in test mode (sensor reading, fault
            // detection), but its output is only used outside test mode.
            const State prev = controller.state();
            controller.update(app::settings, in);
            if (controller.state() != prev && !testing)
                Serial.printf("state: %s -> %s\n", stateName(prev), stateName(controller.state()));

            if (testing) {
                duty = app::test.duty;
                fanOn = app::test.on;
                testOutput = fanOn ? duty : 0;
            } else {
                duty = controller.duty();
                // 4-pin fans keep spinning at 0 % PWM, so cut the supply when idle.
                fanOn = duty > 0;
            }
            invert = app::settings.pwmInvert;

            app::Status& s = app::status;
            s.haveTemp = controller.haveTemp();
            s.temp = controller.temp();
            s.sensorFault = controller.sensorFault();
            s.sensorCount = sensor.count();
            strlcpy(s.sensorId, sensor.id(), sizeof s.sensorId);
            s.duty = duty;
            s.fanOn = fanOn;
            s.pinDuty = invert ? 100.0f - duty : duty;
            s.target = controller.target();
            s.pidOutput = controller.pidOutput();
            s.state = controller.state();
            s.active = controller.active();
            s.testActive = testing;
            s.testRemainingMs = testing ? (uint32_t)(app::test.untilMs - nowMs64) : 0;
            s.rpm = tach.rpm(app::settings.tachPpr);
            s.tachHz = tach.hz();
            s.tachPulses = pulses;
            s.timeValid = in.timeValid;
            s.now = now;
            s.uptimeSec = up;

            avg.add(controller.haveTemp() && !controller.sensorFault(), controller.temp(), fanOn ? duty : 0,
                    testing || controller.active(), dt);
            if ((int32_t)(up - nextHistory) >= 0) {
                app::history.push(avg.take(up));
                nextHistory += HISTORY_INTERVAL_SEC;
            }
        }
        fanApply(duty, fanOn, invert);
    }
}

}  // namespace

namespace app {

void startControlTask() {
    // Higher priority than loop() (1) and the web server, so fades stay smooth.
    xTaskCreate(controlTask, "control", 4096, nullptr, 5, nullptr);
}

}  // namespace app
