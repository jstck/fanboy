This project is microcontroller firmware for driving a fan. It controls the temperature of a bedroom by pumping in fresh air.

"Brain" will be an ESP32 (like an ESP32-C3/S3 or something like that, small and cheap). Measure room temperature using a DS18B20, controlling fan (a couple of 12V 12cm PC fans) using a low-side mosfet.

Needs to do some sort of PWM control (full on/off is a bit disturbing at night).

Web interface to monitor and control the device. Tune set-temp, and maybe PID/PWM settings (depending on control precision needed). Should show graphs of temperature and PWM%.

A weekly schedule for when to turn on/off. It doesn't need to run at all during the day, but start at some certain time in the evening, and then stop regulating at some time in the morning. When turning on/off, the fan should "fade in/out" over a configurable time (maybe 20-30s) to not be a "sudden noise change".

Would like it to build using PlatformIO. It can have the wifi settings configured at build-time (no need just yet for a "wifi setup hotspot portal thingy"), but it needs to be able to handle several wifis (both at home where I'm testing it, and in it's "target environment").