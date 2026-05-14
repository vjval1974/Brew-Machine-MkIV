# Brew Machine MkIV — Raspberry Pi 5 port

TypeScript + Node.js + browser kiosk UI port of the original STM32 / FreeRTOS
Brew Machine MkIV. Targets:

- Raspberry Pi 5 (also runs on Pi 4 / CM4 with the same GPIO interface)
- Official Raspberry Pi 7" DSI touchscreen (800x480) in Chromium kiosk mode
- Raspberry Pi OS Bookworm (64-bit) or later

The original C / FreeRTOS sources in the parent directory are kept untouched.
This folder is the new runtime: a single Node.js process that owns all
hardware (GPIO, I2C, 1-Wire, hardware PWM, flow pulse counting) and serves a
local web UI over WebSocket + HTTP.

## Layout

```
rpi/
├── package.json                npm scripts + dependencies
├── tsconfig.json               server build (src/  → dist/)
├── tsconfig.web.json           browser build (web/src/ → web/js/)
├── config/
│   └── parameters.default.json brew defaults (mash temps, hop times, etc.)
├── src/                        server TypeScript
│   ├── types.ts                AppState, Parameters, BrewStepDef, …
│   ├── index.ts                entry point
│   ├── config/
│   │   ├── pinmap.ts           ALL GPIO assignments — EDIT THIS
│   │   └── i2c.ts              PCF8574 IO-expander addresses
│   ├── hal/                    libgpiod, i2c-bus, w1, pwm, debounce
│   ├── controllers/            valves, pumps, HLT, boil, crane, brew, …
│   ├── state/                  central typed store + change events
│   ├── parameters/             JSON-persisted brew parameters
│   ├── server/                 Express + ws + REST API
│   └── util/                   logger, event bus
├── web/
│   ├── index.html              kiosk page (loads /js/*.js)
│   ├── styles.css
│   ├── src/                    browser TypeScript
│   └── js/                     compiled output (gitignored)
├── dist/                       compiled server (gitignored)
├── data/                       persisted state (gitignored, created at runtime)
├── systemd/                    brew-machine.service + brew-kiosk.service
└── scripts/                    install.sh, kiosk-start.sh
```

## How the brew engine works

Each step in `src/controllers/brew.ts` carries a `wait` flag. This mirrors
the `ucWait` column in the original `brew.c#BrewSteps[]` (line 2056). When
`wait: false` the step is launched and the driver immediately advances; when
`wait: true` the driver first awaits **all previously-launched steps** to
complete before launching the next. This is what makes the original brew
roughly 3 hours faster than a naïve sequential run — the HLT is reheating
the next batch of sparge water *during* the current mash / sparge.

The big concurrent overlaps:

- `Fill+Heat:Strike` (wait=false) runs in parallel with `Grind Grains` (wait=false).
  `DrainHLTForMash` (wait=true) joins both.
- `Fill+Heat:Sparge1` (wait=true → starts) runs in parallel with `Mash` (wait=false).
- `Fill+Heat:Sparge2` (wait=true → starts) runs in parallel with `Sparge1` (wait=false).
- `Fill+Heat:Clean` (wait=false) runs in parallel with `BringToBoil` (wait=false).

The BREW view in the web UI marks waited steps with a clock icon, so you can
see the parallelism at a glance.

## Wiring the Pi

**All GPIO assignments are in `src/config/pinmap.ts`.** Every signal is
currently a placeholder (`bcm: null`) — assign BCM pin numbers there to match
your wiring before running with hardware. The HAL refuses to claim a null
pin unless `MOCK_HARDWARE=1` is set.

The original board uses a PCF8574 I2C IO expander for things the STM32
couldn't reach directly (crane relays, stir motor, hop dropper, boil valve
relays). On the Pi you have 40 GPIO pins, so most of those signals can move
to native GPIO. Pick per-signal in `pinmap.ts`.

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
# 1. Enable 1-Wire, I2C, hardware PWM (raspi-config does the first two)
sudo raspi-config         # Interface Options -> 1-Wire (on), I2C (on)
echo 'dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4' | sudo tee -a /boot/firmware/config.txt

# 2. Install deps
sudo apt update
sudo apt install -y nodejs npm libgpiod-dev i2c-tools chromium-browser unclutter curl

# 3. Install brew-machine
cd /home/pi
git clone <this-repo> Brew-Machine-MkIV
cd Brew-Machine-MkIV/rpi
npm install
npm run build

# 4. Edit pin map
$EDITOR src/config/pinmap.ts          # assign BCM pin numbers for every TODO
npm run build:server                  # recompile

# 5. Test in mock mode first
MOCK_HARDWARE=1 npm run dev
# open http://<pi-ip>:8080 in a desktop browser to verify UI

# 6. Install systemd units + kiosk autostart
sudo bash scripts/install.sh
sudo systemctl enable --now brew-machine
sudo systemctl enable --now brew-kiosk
```

## Running

```bash
npm run build           # build server + web bundles
npm start               # production (runs dist/index.js)
npm run dev             # tsx watch mode, autoreloads on file changes
MOCK_HARDWARE=1 npm run dev   # no GPIO/I2C/W1 needed for UI development
npm run typecheck       # tsc --noEmit on both projects
```

The web UI is served on `http://localhost:8080`. The Pi 7" kiosk opens that
URL fullscreen at boot via `brew-kiosk.service`.

## What was ported

| C source            | TypeScript equivalent                       |
|---------------------|---------------------------------------------|
| `main.c`            | `src/index.ts`                              |
| `valves.c`          | `src/controllers/valves.ts`                 |
| `mash_pump.c`       | `src/controllers/mashPump.ts`               |
| `chiller_pump.c`    | `src/controllers/chillerPump.ts`            |
| `mill.c`            | `src/controllers/mill.ts`                   |
| `stir.c`            | `src/controllers/stir.ts`                   |
| `crane.c`           | `src/controllers/crane.ts`                  |
| `hop_dropper.c`     | `src/controllers/hopDropper.ts`             |
| `hlt.c`             | `src/controllers/hlt.ts`                    |
| `boil.c`            | `src/controllers/boil.ts`                   |
| `boil_valve.c`      | `src/controllers/boilValve.ts`              |
| `Flow1.c`           | `src/controllers/flow.ts`                   |
| `MashWater.c`       | `src/controllers/mashWater.ts`              |
| `parameters.c`      | `src/parameters/parameters.ts`              |
| `brew.c`            | `src/controllers/brew.ts` (parallel WAIT)   |
| `drivers/ds1820.c`  | `src/hal/onewire.ts`                        |
| `I2C-IO.c`          | `src/hal/i2c.ts`                            |
| LCD/touch UI        | `web/` (browser kiosk)                      |
| FreeRTOS tasks      | async loops + EventEmitter                  |
| FreeRTOS queues     | `Promise<void>` returned from commands      |

## Safety

- HLT heating is disabled whenever HLT level reads LOW (background monitor,
  port of `vTaskHLTLevelChecker`).
- INLET valve is forced closed if it stays open for >3 s while HLT level
  reads HIGH.
- Brew QUIT aborts all in-flight HLT command promises and sets every
  controller to safe states.
- On SIGINT / SIGTERM / uncaughtException, every output is driven low before
  the GPIO chip is released.

## Development without a Pi

`MOCK_HARDWARE=1` swaps the libgpiod / i2c-bus / 1-wire backends for
in-memory mocks. The UI, REST API, parameter persistence, and brew state
machine all work normally; only the physical I/O is stubbed. The
Diagnostics tab has buttons to flip level switches, bump temps, and inject
flow pulses so you can drive a complete brew on a laptop without any
hardware.
