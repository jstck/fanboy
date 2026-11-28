#pragma once

// ---- Pins (Waveshare ESP32-C6-Zero) ----
// Header GPIOs: 0-5, 14, 15, 16 (TX), 17 (RX), 18-22.
// Avoid 4, 5, 15 (strapping/JTAG, sampled at reset) and 16/17 (boot log on TX)
// for anything that matters at boot. 8 = onboard RGB LED, 9 = BOOT button,
// 12/13 = USB: not on the header anyway.
#define PIN_DS18B20 0      // 1-wire data, 4.7k pull-up to 3.3V
#define PIN_FAN_PWM 18     // 4-pin: to the open-collector driver for fan pin 4 (PWM)
                           // 2/3-pin: to the MOSFET that switches the fans' supply
#define PIN_FAN_TACH 19    // from fan pin 3 (tach), pull-up to 3.3V. Optional
#define PIN_FAN_SWITCH 20  // 4-pin: drives the transistor that switches the high-side
                           // P-MOSFET. Held off with 2/3-pin fans (fanType setting)

// 1 if the switch GPIO goes HIGH to turn the fan supply on.
#define FAN_SWITCH_ACTIVE_HIGH 1
// PWM inversion (for the open-collector driver) is a setting: pwmInvert.

// ---- Fan PWM ----
// Intel 4-pin fan spec: 21-28 kHz. Most 2/3-pin fans also run fine with their
// supply chopped at 25 kHz (silent, but their tach reads garbage while PWMed).
// If one misbehaves, try a low frequency like 30-100 Hz instead (may tick).
#define FAN_PWM_FREQ_HZ 25000
#define FAN_PWM_BITS 10

// ---- Tach ----
#define TACH_MIN_INTERVAL_US 1000  // ignore tach edges closer than this (noise)

// ---- Test mode ----
// The test page renews its lease every second. When renewals stop (page
// closed, laptop asleep, WiFi gone) the firmware returns to automatic control
// this long after the last one.
#define TEST_LEASE_MS 5000

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
