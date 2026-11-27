#pragma once
// Hardware test firmware config.
// Copy this file to hwtest_config.h (gitignored, same folder) and edit.

// ---- WiFi ----
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
#define HOSTNAME "fanboy-test"  // http://fanboy-test.local

// ---- IP ----
// 0 = DHCP. 1 = static, using the addresses below.
#define USE_STATIC_IP 0
#define STATIC_IP "192.168.1.50"
#define STATIC_GATEWAY "192.168.1.1"
#define STATIC_SUBNET "255.255.255.0"
#define STATIC_DNS1 "192.168.1.1"
#define STATIC_DNS2 "1.1.1.1"

// ---- Pins (Waveshare ESP32-C6-Zero) ----
// Header GPIOs: 0-5, 14, 15, 16 (TX), 17 (RX), 18-22.
// Avoid 4, 5, 15 (strapping/JTAG, sampled at reset) and 16/17 (boot log on TX)
// for anything that matters at boot. 8 = onboard RGB LED, 9 = BOOT button,
// 12/13 = USB: not on the header anyway.
#define PIN_DS18B20 0      // 1-wire data, 4.7k pull-up to 3.3V
#define PIN_FAN_PWM 18     // to the open-collector driver for fan pin 4 (PWM)
#define PIN_FAN_TACH 19    // from fan pin 3 (tach), pull-up to 3.3V (not 5V/12V!)
#define PIN_FAN_SWITCH 20  // drives the transistor that switches the high-side P-MOSFET

// ---- Polarity ----
// 1 if the GPIO goes HIGH to turn the fan supply on. True for the usual
// "NPN/N-MOSFET pulls the P-MOSFET gate low" driver.
#define FAN_SWITCH_ACTIVE_HIGH 1
// 1 if the PWM goes through an inverting stage, like an open-collector
// NPN/N-MOSFET pulling the fan's PWM line low. 0 when the GPIO drives the fan's
// PWM pin directly. This is only the default: the web UI can change it, and
// the UI's choice is saved and wins from then on.
#define FAN_PWM_INVERTED 1
// Use the ESP32's internal pull-up (~45k) on the tach pin. An external
// 10k to 3.3V is more robust; leaving this on as well does no harm.
#define TACH_INTERNAL_PULLUP 1

// ---- Fan ----
#define FAN_PWM_FREQ_HZ 25000      // Intel 4-pin spec: 21-28 kHz
#define FAN_PWM_BITS 10
#define DEFAULT_DUTY_PERCENT 50    // PWM duty at boot (the switch always starts off)
#define TACH_MIN_INTERVAL_US 1000  // ignore tach edges closer than this (noise)
