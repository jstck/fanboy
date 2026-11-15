# Fanboy

ESP32-C3 firmware that cools a bedroom by blowing in fresh air with 12 V PC fans.
Inside a weekly night-time schedule, a PI controller adjusts the fan speed toward a set temperature measured by a DS18B20, fading in and out smoothly. A web UI shows live status, 24 h graphs and all settings.

## Hardware

| What | Where |
|---|---|
| DS18B20 data | GPIO4, 4.7 kΩ pull-up to 3.3 V |
| MOSFET gate | GPIO5 via ~100 Ω, 10 kΩ gate→GND pull-down |
| Fans (2/3-pin, in parallel) | +12 V → fan + ; fan − → MOSFET drain ; source → GND |

- Use a **logic-level** N-MOSFET that fully turns on at 3.3 V gate drive (e.g. AO3400, IRLZ44N, IRLB8721).
- Put a **flyback diode** (e.g. 1N5819/SS34) across the fans, cathode to +12 V.
- Add a bulk capacitor (100–470 µF) on the 12 V rail. Power the ESP32 from 12 V with a small buck converter, sharing GND.
- PWM runs at 25 kHz (`include/config.h`). Chopping the supply of a 3-pin fan works with most fans, but each fan has a **stall floor**: find the lowest duty that reliably spins it and set *Min duty* a bit above that. If your fans misbehave at 25 kHz, try a low frequency (30–100 Hz) instead.
- Pins and timing constants live in `include/config.h`.

## Build and flash

```sh
cp include/secrets.example.h include/secrets.h   # then fill in WiFi networks + OTA password
pio run -e c3 -t upload && pio device monitor     # USB
FANBOY_OTA_PASSWORD=... pio run -e c3-ota -t upload   # over WiFi, once it's running
```

`secrets.h` can list any number of networks (home, bedroom, …). The device joins the strongest one in range and reconnects or switches by itself.

The web UI (`web/index.html`) is gzipped into the firmware at build time by `scripts/embed_web.py`, so there's no separate filesystem upload.

## Use

Open <http://fanboy.local> (or the IP printed on the serial monitor).

- **Mode:** *Auto* follows the schedule, *On* regulates now, and *Off* stops. Mode changes fade too.
- **Schedule:** one window per weekday. If the stop time is at or before the start time, the window ends the next morning, so Mon 21:30→07:00 runs until Tuesday 07:00. *Auto* needs NTP time, so set your **time zone** as a POSIX TZ string.
- **Control:** the error is `room − setpoint`. Kp is "% duty per °C too warm". Ki slowly adds duty while the room stays too warm, which removes steady-state offset. Duty is kept within [min, max]. With *Stop fans when cool enough*, the fans stop after the PI has asked for 0 % for 2 min, and restart once it asks for at least min duty. Otherwise they idle at min duty.
- **Sensor fault:** after 3 bad readings in a row while active, the fans run at the fault duty.
- **History:** RAM only, 1-min averages for 24 h. It's lost on reboot.

Settings are stored in NVS and survive reboots and reflashing.

### API

- `GET /api/status`: live state.
- `GET /api/history`: 24 h of 1-min samples, with `t` as device uptime in seconds.
- `GET /api/settings` / `POST /api/settings`: JSON. Partial updates are fine, for example `{"mode":"off"}` or `{"setpoint":19.5}`.

## Tests

The control logic (schedule, PI, fader, controller state machine, settings validation) is plain C++ in `lib/fancore` and is tested on the host:

```sh
pio test -e native
```
