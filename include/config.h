#pragma once

// ---- Pins (ESP32-C3; avoid strapping pins 2, 8, 9) ----
#define PIN_DS18B20 4  // DS18B20 data, 4.7k pull-up to 3.3V
#define PIN_FAN 5      // MOSFET gate (via ~100R, 10k pull-down to GND)

// ---- Fan PWM ----
// 25 kHz is above hearing. If the fans misbehave (some 3-pin fans' internal
// electronics dislike chopped supply), try a low frequency like 30-100 Hz instead,
// at the cost of a faint ticking.
#define FAN_PWM_FREQ_HZ 25000
#define FAN_PWM_BITS 10

// ---- Timing ----
#define CONTROL_TICK_MS 100         // controller/fader update period
#define SENSOR_PERIOD_MS 2000       // DS18B20 sample period
#define SENSOR_CONVERSION_MS 800    // 12-bit conversion takes up to 750 ms
#define HISTORY_INTERVAL_SEC 60     // one averaged history sample per minute
#define HISTORY_SAMPLES (24 * 60)   // 24 h at 1/min

// ---- Network ----
#define HOSTNAME "fanboy"  // http://fanboy.local, OTA target
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
