# Fanboy

ESP32-C6 firmware that cools a bedroom by blowing in fresh air with 12 V PC fans.
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
pio run -e c6 -t upload && pio device monitor     # USB
FANBOY_OTA_PASSWORD=... pio run -e c6-ota -t upload   # over WiFi, once it's running
```

`secrets.h` can list any number of networks (home, bedroom, …). The device joins the strongest one in range and reconnects or switches by itself.

The web UI (`web/index.html`) is gzipped into the firmware at build time by `scripts/embed_web.py`, so there's no separate filesystem upload.

## First-time setup and tuning

1. **Set the time zone** in the web UI. The default is `UTC0`, so the schedule runs at the wrong hours until you change it.
2. **Find the stall floor.** Use *On* mode and lower *Min duty* until the fans stop spinning, then set it a bit above that. The default of 25 % is a guess.
3. **Tune the controller** by watching the graphs over a few nights. The defaults are only a starting point:

   | Setting | Default |
   |---|---|
   | Setpoint | 20 °C |
   | Kp | 30 % per °C |
   | Ki | 0.05 % per °C·s |
   | Kd | 0 |
   | Min / max duty | 25 % / 100 % |
   | Fade time | 30 s (full 0→100 % sweep) |
   | Sensor-fault duty | 40 % |
   | Schedule | every day 21:00 → 07:00 |

   If the temperature oscillates around the setpoint, lower Kp or Ki. If it stays above the setpoint for long stretches while the fans aren't at max, raise Ki.
4. If the fans whine or behave oddly at 25 kHz, change `FAN_PWM_FREQ_HZ` in `include/config.h` (e.g. to 50) and reflash. Expect a faint ticking instead.

## Use

Open <http://fanboy.local> (or the IP printed on the serial monitor). The page shows live status and temperature and fan-duty graphs over the last 1, 6 or 24 h.

- **Mode:** *Auto* follows the schedule, *On* regulates now, and *Off* stops. Mode changes fade too.
- **Schedule:** one window per weekday. If the stop time is at or before the start time, the window ends the next morning, so Mon 21:30→07:00 runs until Tuesday 07:00. *Auto* needs NTP time, so set your **time zone** as a POSIX TZ string.
- **Control:** the error is `room − setpoint`. Kp is "% duty per °C too warm". Ki slowly adds duty while the room stays too warm, which removes steady-state offset. Duty is kept within [min, max]. With *Stop fans when cool enough*, the fans stop after the PI has asked for 0 % for 2 min, and restart once it asks for at least min duty. Otherwise they idle at min duty.
- **Sensor fault:** after 3 bad readings in a row while active, the fans run at the fault duty.
- **History:** RAM only, 1-min averages for 24 h. It's lost on reboot.

Settings are stored in NVS and survive reboots and reflashing.

### How it runs

The DS18B20 is sampled every 2 s. Fan control runs in its own FreeRTOS task, updating every 100 ms, separate from WiFi and the web server. A WiFi reconnect, which can block for several seconds while scanning, therefore can't stall a fade or the regulation.

### API

- `GET /api/status`: live state.
- `GET /api/history`: 24 h of 1-min samples, with `t` as device uptime in seconds.
- `GET /api/settings` / `POST /api/settings`: JSON. Partial updates are fine, for example `{"mode":"off"}` or `{"setpoint":19.5}`.

## Hardware test firmware

`hwtest/` is a separate, minimal firmware for checking the wiring on a breadboard: it shows the DS18B20 reading and lets you switch a 4-pin fan on and off, set its PWM duty (25 kHz) and see its tach speed. It has no regulation and no schedule.

```sh
cp hwtest/include/hwtest_config.example.h hwtest/include/hwtest_config.h   # WiFi, DHCP/static IP, pins, polarities
pio run -d hwtest -t upload && pio device monitor -d hwtest
```

Then open `http://fanboy-test.local` or the IP shown on the serial monitor. *Pulses per revolution* (default 2) and *Invert PWM output* (for an inverting driver stage such as an open-collector transistor) are set in the UI and kept across reboots. The config file also has notes on which ESP32-C6-Zero pins to use.

## Tests

The control logic (schedule, PI, fader, controller state machine, settings validation) is plain C++ in `lib/fancore` and is tested on the host:

```sh
pio test -e native
```
