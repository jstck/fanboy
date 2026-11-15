// Control task: sensor -> controller -> fan PWM, plus history recording.
#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#include "app.h"

using namespace fancore;

namespace {

// Non-blocking DS18B20 reader: requests a conversion, reads it back later.
class TempSensor {
public:
    explicit TempSensor(uint8_t pin) : wire_(pin), dallas_(&wire_) {}

    void begin() {
        dallas_.begin();
        dallas_.setWaitForConversion(false);
        dallas_.setResolution(12);
        Serial.printf("DS18B20: %d device(s) found\n", dallas_.getDeviceCount());
    }

    // Returns true when a sample (valid or not) completed during this call.
    bool poll(uint32_t nowMs, float& temp, bool& valid) {
        if (!converting_) {
            if (nowMs - lastRequest_ < SENSOR_PERIOD_MS) return false;
            lastRequest_ = nowMs;
            if (dallas_.getDeviceCount() == 0) {
                dallas_.begin();  // sensor missing/replugged: rescan
                dallas_.setResolution(12);
                if (dallas_.getDeviceCount() == 0) {
                    valid = false;
                    return true;
                }
            }
            dallas_.requestTemperatures();
            converting_ = true;
            return false;
        }
        if (nowMs - lastRequest_ < SENSOR_CONVERSION_MS) return false;
        converting_ = false;
        const float t = dallas_.getTempCByIndex(0);
        // -127 = disconnected/CRC error; 85 = power-on reset value (conversion didn't happen).
        valid = t != DEVICE_DISCONNECTED_C && t != 85.0f && t > -40 && t < 80;
        temp = t;
        return true;
    }

private:
    OneWire wire_;
    DallasTemperature dallas_;
    bool converting_ = false;
    uint32_t lastRequest_ = 0;
};

void fanBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_FAN, FAN_PWM_FREQ_HZ, FAN_PWM_BITS);
#else
    ledcSetup(0, FAN_PWM_FREQ_HZ, FAN_PWM_BITS);
    ledcAttachPin(PIN_FAN, 0);
#endif
}

void fanWrite(float dutyPct) {
    const uint32_t maxVal = (1u << FAN_PWM_BITS);
    uint32_t v;
    if (dutyPct < 0.5f) v = 0;
    else if (dutyPct >= 99.5f) v = maxVal;  // fully on (LEDC treats 2^bits as 100 %)
    else v = (uint32_t)(dutyPct / 100.0f * maxVal + 0.5f);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_FAN, v);
#else
    ledcWrite(0, v);
#endif
}

void controlTask(void*) {
    TempSensor sensor(PIN_DS18B20);
    sensor.begin();
    fanBegin();
    fanWrite(0);

    Controller controller;
    HistoryAverager avg;
    uint32_t last = millis();
    uint32_t nextHistory = app::uptimeSec() + HISTORY_INTERVAL_SEC;
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

        const uint32_t up = app::uptimeSec();
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

            const State prev = controller.state();
            controller.update(app::settings, in);
            if (controller.state() != prev)
                Serial.printf("state: %s -> %s\n", stateName(prev), stateName(controller.state()));

            app::Status& s = app::status;
            s.haveTemp = controller.haveTemp();
            s.temp = controller.temp();
            s.sensorFault = controller.sensorFault();
            s.duty = controller.duty();
            s.target = controller.target();
            s.pidOutput = controller.pidOutput();
            s.state = controller.state();
            s.active = controller.active();
            s.timeValid = in.timeValid;
            s.now = now;
            s.uptimeSec = up;

            avg.add(controller.haveTemp() && !controller.sensorFault(), controller.temp(),
                    controller.duty(), controller.active(), dt);
            if ((int32_t)(up - nextHistory) >= 0) {
                app::history.push(avg.take(up));
                nextHistory += HISTORY_INTERVAL_SEC;
            }
        }
        fanWrite(controller.duty());
    }
}

}  // namespace

namespace app {

void startControlTask() {
    // Higher priority than loop() (1) and the web server, so fades stay smooth.
    xTaskCreate(controlTask, "control", 4096, nullptr, 5, nullptr);
}

}  // namespace app
