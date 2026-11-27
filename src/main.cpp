#include <Arduino.h>

#include "app.h"

namespace app {

fancore::Settings settings;
Status status;
History history;
TestMode test;

static SemaphoreHandle_t mutex;

void initLock() { mutex = xSemaphoreCreateMutex(); }
void lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(mutex); }

uint32_t uptimeSec() { return (uint32_t)(esp_timer_get_time() / 1000000LL); }
uint64_t nowMs() { return (uint64_t)(esp_timer_get_time() / 1000LL); }

}  // namespace app

void setup() {
    // Fan supply off first thing. Until here the pin floats; the switch
    // driver's pull-down keeps the fans off during reset and boot.
    pinMode(PIN_FAN_SWITCH, OUTPUT);
    digitalWrite(PIN_FAN_SWITCH, FAN_SWITCH_ACTIVE_HIGH ? LOW : HIGH);

    Serial.begin(115200);
    delay(200);
    Serial.println("\nfanboy starting");

    app::initLock();
    app::loadSettings();
    app::startControlTask();  // fan control runs independently of networking
    app::netBegin();
    app::webBegin();
}

void loop() {
    app::netLoop();
    delay(10);
}
