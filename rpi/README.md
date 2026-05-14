# Brew Machine MkIV — Raspberry Pi 5 port

Node.js + web UI port of the original STM32 / FreeRTOS Brew Machine MkIV.

Targets:

- Raspberry Pi 5 (also runs on Pi 4 / CM4 with the same GPIO interface)
- Official Raspberry Pi 7" DSI touchscreen (800x480), launched in Chromium kiosk mode
- Raspberry Pi OS Bookworm (64-bit) or later

The original C / FreeRTOS sources in the parent directory are kept untouched.
This folder is the new runtime: a single Node.js process that owns all hardware
(GPIO, I2C, 1-Wire, hardware PWM, flow pulse counting) and serves a local web
UI over WebSocket + HTTP. A Chromium kiosk on the Pi connects to
`http://localhost:8080` on boot.

## Layout

```
rpi/
├── package.json               npm dependencies and scripts
├── config/
│   ├── pinmap.js              ALL GPIO assignments live here — EDIT THIS
│   ├── i2c.js                 PCF8574 IO-expander port addresses
│   └── parameters.default.json brew defaults (mash temps, hop times, etc.)
├── src/
│   ├── index.js               entry point — wires HAL + controllers + server
│   ├── hal/                   hardware abstraction (libgpiod, i2c-bus, w1, pwm)
│   ├── controllers/           valves, pumps, HLT, boil, crane, brew, ...
│   ├── state/                 central state store + event bus
│   ├── parameters/            JSON-persisted brew parameters
│   ├── server/                Express + ws + REST API
│   └── util/                  logger, helpers
├── web/                       static client (HTML / CSS / JS) for kiosk
├── systemd/                   brew-machine.service + brew-kiosk.service
└── scripts/                   install.sh, kiosk-start.sh
```

## Wiring the Pi

**All GPIO assignments are in `config/pinmap.js`.** Every signal is currently
a placeholder (`TODO`) — assign BCM pin numbers there to match your wiring
before running with hardware. The HAL refuses to claim a line marked `null`
unless `MOCK_HARDWARE=1` is set.

The original board uses a PCF8574 I2C IO expander for things the STM32 couldn't
reach directly (crane relays, stir motor, hop dropper, boil valve relays). On
the Pi you have 40 GPIO pins, so most of those signals can move to native GPIO
— but the expander is still useful and the code supports either. Decide
per-signal in `config/pinmap.js`.

Sensors / outputs from the original system:

| Signal              | Original     | Suggested Pi mapping              |
|---------------------|--------------|-----------------------------------|
| HLT SSR             | GPIO         | Native GPIO (relay/SSR)           |
| Boil SSR            | TIM4 PWM     | Hardware PWM (GPIO12 or GPIO18)   |
| HLT temp (DS18B20)  | 1-Wire       | Kernel `w1-gpio` on GPIO4         |
| Mash temp (DS18B20) | 1-Wire       | Same 1-Wire bus, different ROM    |
| HLT level low/high  | Float switch | Native GPIO inputs, pull-up       |
| Flow sensor         | EXTI pulses  | GPIO with edge events (libgpiod)  |
| Valves x4           | GPIO         | Native GPIO (relay board)         |
| Mash pump           | GPIO         | Native GPIO                       |
| Chiller pump        | GPIO         | Native GPIO                       |
| Mill                | GPIO         | Native GPIO                       |
| Crane up/down       | I2C PCF8574  | Native GPIO or keep on PCF8574    |
| Stir                | I2C PCF8574  | Native GPIO or keep on PCF8574    |
| Hop dropper drive   | I2C PCF8574  | Native GPIO or keep on PCF8574    |
| Hop dropper limit   | GPIO         | Native GPIO input, pull-up        |
| Boil valve relays   | I2C PCF8574  | Native GPIO or keep on PCF8574    |
| Boil valve limits   | GPIO         | Native GPIO inputs                |

## First-time setup on the Pi

```bash
# 1. Enable 1-Wire, I2C, hardware PWM
sudo raspi-config         # Interface Options -> 1-Wire (on), I2C (on)
echo 'dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4' | sudo tee -a /boot/firmware/config.txt
# (or use sysfs PWM via dtoverlay=pwm)

# 2. Install deps
sudo apt update
sudo apt install -y nodejs npm libgpiod-dev i2c-tools chromium-browser unclutter

# 3. Install brew-machine
cd /home/pi
git clone <this-repo> Brew-Machine-MkIV
cd Brew-Machine-MkIV/rpi
npm install
cp config/parameters.default.json data/parameters.json   # creates persistent copy

# 4. Edit pin map
$EDITOR config/pinmap.js   # assign BCM pin numbers for every TODO

# 5. Test in mock mode first
MOCK_HARDWARE=1 npm start
# open http://localhost:8080 in a desktop browser to verify UI

# 6. Install systemd units + kiosk autostart
sudo bash scripts/install.sh
sudo systemctl enable --now brew-machine
sudo systemctl enable --now brew-kiosk
```

## Running

```bash
npm start           # production
npm run dev         # nodemon, autoreloads on file changes
MOCK_HARDWARE=1 npm start   # no GPIO/I2C/W1 access; UI works for development
```

The web UI is served on `http://localhost:8080`. The Pi 7" kiosk opens this
URL fullscreen at boot via `brew-kiosk.service`.

## What was ported

Every C controller has a direct JS counterpart:

| C source        | JS equivalent                          |
|-----------------|----------------------------------------|
| `main.c`        | `src/index.js`                         |
| `valves.c`      | `src/controllers/valves.js`            |
| `mash_pump.c`   | `src/controllers/mashPump.js`          |
| `chiller_pump.c`| `src/controllers/chillerPump.js`       |
| `mill.c`        | `src/controllers/mill.js`              |
| `stir.c`        | `src/controllers/stir.js`              |
| `crane.c`       | `src/controllers/crane.js`             |
| `hop_dropper.c` | `src/controllers/hopDropper.js`        |
| `hlt.c`         | `src/controllers/hlt.js`               |
| `boil.c`        | `src/controllers/boil.js`              |
| `boil_valve.c`  | `src/controllers/boilValve.js`         |
| `Flow1.c`       | `src/controllers/flow.js`              |
| `MashWater.c`   | `src/controllers/mashWater.js`         |
| `parameters.c`  | `src/parameters/parameters.js`         |
| `brew.c`        | `src/controllers/brew.js`              |
| `drivers/ds1820.c` | `src/hal/onewire.js`                |
| `I2C-IO.c`      | `src/hal/i2c.js`                       |
| LCD/touch UI    | `web/` (browser kiosk)                 |
| FreeRTOS tasks  | Node async loops + EventEmitter        |

The `brew.js` state machine preserves the step sequence from the original
`BrewSteps[]` array but expresses each step as an async function instead of a
poll-and-callback table. Functional behavior (HLT fill → mash strike → mash →
mash-out → sparge → boil with timed hop additions → chill → pump to fermenter)
is identical.

## Safety

- HLT heating is disabled whenever HLT level reads LOW (same as original
  `vTaskHLTLevelChecker`).
- INLET valve is forced closed if it stays open for >3 s while HLT level reads
  HIGH (same).
- A reset on the brew aborts all controllers via `vBrewSetSafeStates`
  equivalent in `brew.js#safeStates()`.
- On Node.js process exit (SIGINT/SIGTERM/uncaughtException), every output is
  driven low before the GPIO chip is released.

## Development without a Pi

`MOCK_HARDWARE=1` swaps the libgpiod / i2c-bus / 1-wire backends for in-memory
mocks. The UI, REST API, parameters persistence, and brew state machine all
work normally; only the physical I/O is stubbed. Useful for editing the web
UI on a laptop.
