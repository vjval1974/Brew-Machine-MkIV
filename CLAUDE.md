# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working with me (advisor mode)

You are my advisor, not my assistant. Be direct and prioritize accuracy over agreeableness. Follow these rules:

- **Lead with the most useful thing you can say.** No warm-up paragraphs, no "there are several ways to look at this." If there's something I probably don't want to hear, say it in the first line.
- **Evaluate before you agree.** Don't open with affirmation by default. If I'm right, say so plainly and move on. If I'm wrong, missing something, or making an unstated assumption, say that instead. Agreement should be earned by the merits, not withheld as a reflex.
- **When you disagree, give me structure:** why I'm wrong, what you'd do instead, and the specific downside of my approach.
- **Rate your confidence.** Tag claims `[Certain]` for hard evidence, `[Likely]` for strong inference, `[Guessing]` when filling gaps. If most of a reply is guesswork, say so up front.
- **Cut filler phrases:** "Great question," "You're absolutely right," "That makes a lot of sense," "Absolutely," "Definitely." If you catch yourself writing one, delete it.
- **Hold your position under pushback** unless I give you genuinely new information. Repetition and insistence ("but I really think") aren't new information. But if I show you you're wrong, change your mind and say why.

## What this is

Firmware for **Brew Machine MkIV** (project name `RTOSBrew`): a fully automated home brewery controller. It runs on an **STM32F103 high-density** MCU (ARM Cortex-M3 @ 72 MHz) under **FreeRTOS**, driving a resistive touchscreen LCD UI plus pumps, valves, heating elements (SSRs), a motorised grain crane, mill, stirrer, hop dropper, flow meter and DS1820 temperature sensors. There is no host/PC component — everything here is cross-compiled C that runs on the board.

## Build, flash, run

The real build is the hand-written `Makefile` (the `CMakeLists.txt` is almost entirely commented out and not used; the `Debug/` and `Release/` directories contain Eclipse-CDT–generated makefiles and committed build artifacts — prefer the top-level `Makefile`).

Toolchain: `arm-none-eabi-gcc` must be on `PATH`.

```sh
make            # compile + link -> RTOSBrew.axf, then objcopy -> Debug/RTOSBrew.bin
make clean      # remove objects, .axf, .bin, .map
make log        # symbol table + memory listing summaries (uses arm-none-eabi-size/nm)
```

Flashing (these targets depend on hardware/host tooling, won't work in CI):

```sh
make install0   # flash over serial /dev/ttyUSB0 via stm32loader.py
make install1   # flash over serial /dev/ttyUSB1
make jtag       # flash via OpenOCD telnet console on localhost:4444 (mass_erase + write_bank)
make run        # jtag flash then "reset run"
```

OpenOCD/JTAG config lives in `jtag/` (`busblaster.cfg`, `stm_board.cfg`); see `openocd.txt`.

**There is no automated test suite and no linter** — this is bare-metal firmware verified on hardware. Key compile-time defines (set in the Makefile `CFLAGS`): `STM32F10X_HD`, `USE_STDPERIPH_DRIVER`, `VECT_TAB_FLASH`, `GCC_ARMCM3`. Standard is `gnu99`, optimisation `-O0`, no `--gc-sections` (kept off so the debugger copes). Linker script: `stm32_flash.ld`.

When adding a new `.c` file, you must add it to the `SOURCE` list in the `Makefile` — there is no globbing.

## FreeRTOS configuration

See `FreeRTOSConfig.h`: 72 MHz CPU clock, 1 ms tick (`configTICK_RATE_HZ = 1000`), only **5 priority levels** (`configMAX_PRIORITIES = 5`), `configMINIMAL_STACK_SIZE = 256` words, and a **53 KB** heap (`heap_2.c` allocator — supports free but not coalescing). Heap is tight; watch `xPortGetFreeHeapSize()` and stack high-water marks.

## Architecture

### Startup
`main.c` is the entry point. `prvSetupHardware()` configures clocks (HSE → PLL ×9 → 72 MHz), enables GPIO/peripheral clocks and the vector table, then `main()` initialises every module (`v*Init()`), builds the menu tree, creates the FreeRTOS tasks, and calls `vTaskStartScheduler()`. The three hooks at the bottom of `main.c` (`vApplicationStackOverflowHook`, `vApplicationMallocFailedHook`, `vApplicationIdleHook`) and `vCheckTask` are the health/diagnostics layer — `vCheckTask` periodically prints task stack high-water marks and heap deltas over serial.

### Module pattern ("applets")
Each functional area (HLT, boil, crane, mill, stir, hop dropper, valves, mash pump, chiller pump, boil valve, flow meter, etc.) is a self-contained module exposing a consistent set of functions, wired into the UI through the menu system:

- `v<Name>Init(void)` — called once from `main()`.
- `v<Name>Applet(int init)` — the screen for that module; `init` is 1 on entry/activate, 0 on deactivate.
- `i<Name>Key(int xx, int yy)` or `<Name>Key(int xx, int yy)` — touch handler; returns non-zero when the touch is "consumed" / should exit the applet.
- Often a dedicated FreeRTOS task `vTask<Name>` / `v<Name>Task` doing the real-time control work.

Modules talk to each other and to their tasks via **FreeRTOS queues** carrying small command/message structs (see `message.h`, plus per-module structs like `HltMessage`, `BrewMessage`). Avoid touching hardware directly from another module — send a message to that module's task instead.

### Menu / UI system (`menu.c`, `menu.h`)
The UI is a tree of `struct menu` arrays (defined statically in `main.c`: `main_menu`, `manual_menu`, `diag_menu`). Each entry has `{ text, next (submenu), activate(int), press_handler(uchar), touch_handler(int,int) }`. The menu engine handles drawing (one- or two-column layouts), breadcrumb navigation, "Back" entries, and dispatching touches either to a submenu or to a module's applet/touch handler. `menu_command(item)` lets non-touch sources (e.g. serial) drive the menu programmatically.

### Brew state machine (`brew.c`) — the heart of the system
`brew.c` (large, ~65 KB) orchestrates a full brew. The sequence is the static `BrewSteps[]` array; each `BrewStep` is:

```c
{ pcStepName, setupFunc(void*), poll(void*), iFuncParams[5], uTimeout, uStartTime, uElapsedTime, ucComplete, ucWait }
```

`vTaskBrew` walks the array step by step: it calls the step's `setupFunc` once, then repeatedly calls `poll` until the step signals complete (or times out via `uTimeout`, in seconds). `ucWait` means "don't start until previous asynchronous steps finished." Steps drive the other modules by sending queue messages (e.g. `vBrewHLTSetupFunction` sends `HLT_CMD_HEAT_AND_FILL`, crane/mill/boil/sparge setup functions, etc.). The brew applet itself has several sub-screens (`MAIN_APPLET`, `GRAPH_APPLET`, `STATS_APPLET`, `RESUME_APPLET`, `QUIT_APPLET`) selected via on-screen buttons whose pixel rectangles are `#define`d in `brew.h`. Editing the recipe = editing `BrewSteps[]` and/or `BrewParameters`.

### Recipe parameters (`parameters.c`, `parameters.h`)
All tunable recipe/timing values live in one global `struct Parameters BrewParameters` (strike/sparge/mash-out temps and litres, mash/boil/chill times, hop addition times, pump/stir priming, etc.). The Parameters applet edits these on-device. Read recipe values from `BrewParameters` rather than hard-coding.

### Hardware I/O layers
- **Discrete GPIO**: valves and the HLT SSR are driven directly on STM32 GPIO pins — pin/port mappings are `#define`d in the relevant header (e.g. `valves.h`, `hlt.h`).
- **I2C IO expanders (`I2C-IO.c`)**: banks of relays/outputs hang off **PCF8574** expanders at I2C addresses `0x70`–`0x7E`, aliased `PORTU`–`PORTY`. Set/clear individual outputs with `vPCF_SetBits(pin, addr)` / `vPCF_ResetBits(pin, addr)`; read inputs with `cI2cGetInput()`. A dedicated `vI2C_SendTask` serialises bus access.
- **Drivers** (`drivers/`): `lcd.c` (FSMC-attached display, 320×240), `touch.c`, `adc.c`, `ds1820.c` (1-Wire temperature probes), `serial.c` (USART command/console), `speaker.c`, `timer.c`, `leds.c`, `button.c`, `buffer.c`.

### Console & remote control
- Printing is asynchronous: call `vConsolePrint(str)` which enqueues onto `xPrintQueue`; `vConsolePrintTask` drains it to the USART. Don't `printf` directly from time-critical paths.
- A USART **serial control task** (`vSerialControlCentreTask` in `drivers/serial.c`) parses inbound line commands — e.g. `sb` starts a brew remotely (drives the menu via `menu_command` then `vBrewRemoteStart()`), `STOP` is a hook for stopping. Extend remote control here.

## Vendored / third-party code (avoid editing)
- `FreeRTOS/Source/` — FreeRTOS kernel (list/queue/tasks + ARM_CM3 port + `heap_2`).
- `std_periph_drivers/` — ST Standard Peripheral Library for STM32F10x.
- `CM3/` — CMSIS core, startup, system init, interrupt vectors (`stm32f10x_it.c`).
- `printf-stdarg.c`, `stf_syscalls_minimal.c`, `newlibstubs.c` — minimal libc/printf and syscall stubs for the bare-metal target.

Treat these as upstream; application logic belongs in the top-level `*.c`/`*.h` and `drivers/`.

## Conventions
- **Naming** is Hungarian-style and pervasive — match it: `v` = void function, `x` = handle or struct-typedef value (`xTaskHandle`, `xQueueHandle`), `pc` = `char*`/string, `uc` = `unsigned char`, `ui`/`u` = `unsigned int`, `i` = `int`, `f` = `float`, `d` = `double`, `p` = pointer. Tasks are `vTask…`/`v…Task`; init functions `v…Init`; applet entry `v…Applet`; touch handlers `…Key`.
- Strings passed to the console/LCD are frequently null-terminated explicitly as `"...\r\n\0"`.
- LCD button/region hit-boxes are defined as `X1/Y1/X2/Y2` pixel-rectangle macro groups in the owning header (see `brew.h`, `hlt.h`).
- Licensed under **GPL v3** (`LICENSE`); existing files carry author/date headers — keep the style when adding new modules.

## Raspberry Pi 5 port (`rpi/`)

A second, **parallel** implementation lives under `rpi/` (added on branch `claude/raspberry-pi-lcd-port-WAjwk`). It reimplements the whole controller in **TypeScript / Node.js (≥20)** for a **Raspberry Pi 5** with a browser kiosk UI instead of the FSMC LCD + touch. The C firmware is the authoritative source of behaviour; the Pi port is a faithful re-port, not a replacement — keep them in sync conceptually when changing brew logic.

It is fully additive: nothing outside `rpi/` is modified, so the two targets don't conflict.

### Build / run (from `rpi/`)
```sh
npm install            # express + ws; node-libgpiod & i2c-bus are optionalDependencies
npm run build          # tsc server (src→dist) + web (web/src→web/js)
npm start              # production: node dist/index.js  (UI on :8080)
npm run dev            # tsx watch mode
MOCK_HARDWARE=1 npm run dev   # no GPIO/I2C/1-Wire needed — in-memory HAL mocks
npm run typecheck      # tsc --noEmit on both tsconfigs (server + web)
npm run test:safety    # MOCK_HARDWARE=1 brew-abort/safe-state regression test (exits non-zero on fail)
```
`tsconfig.json` is `strict`. Typecheck and `test:safety` both currently pass — treat a regression in either as a blocker.

### Architecture mapping (C → TS)
Source is split into **`src/domains/`** (the controllers, grouped by area, each with an `index.ts` barrel) and **`src/platform/`** (cross-cutting infra). Mapping from the C modules:
- `main.c` → `src/index.ts` (init order + clock/peripheral setup → HAL/controller wiring + graceful shutdown).
- One module per C file, grouped by domain under `src/domains/`:
  - `brewing/brew.ts` (the step machine), `hlt/hlt.ts`, `boil/boil.ts`
  - `hydraulics/` → `valves`, `mashPump`, `chillerPump`, `boilValve`, `flow`, `mashWater`
  - `motion/` → `crane`, `mill`, `stir`, `hopDropper`
  - `sensing/` → `tempSensors`
- HAL + infra under `src/platform/`: `hal/gpio.ts` (libgpiod), `hal/i2c.ts` (PCF8574 expanders), `hal/onewire.ts` (DS18B20 via kernel `w1-therm`), `hal/pwm.ts` (sysfs hardware PWM for the boil SSR), `hal/watchdog.ts`, and `hal/workers/flow-worker.ts` (worker thread counting flow pulses on a `SharedArrayBuffer`). Central typed state is `platform/store/store.ts` (`AppState`); brew recipe params are `platform/parameters/parameters.ts` + `config/parameters.default.json`, one-to-one with `parameters.h`.
- FreeRTOS tasks → async loops; FreeRTOS queues → `Promise<void>` returned from controller commands; LCD/menu UI → `web/` (Express + WebSocket; state pushed as snapshot/patch messages).

> Note: the older `rpi/README.md` still describes the pre-reorg flat `src/controllers/` + `src/hal/` layout. Trust the tree above (and `src/domains/` / `src/platform/`) over the README until it's updated.

### Two things to know before touching it
- **`src/config/pinmap.ts` ships with every `bcm` set to `null`** (placeholders). On real hardware the HAL refuses to claim a null pin, and `index.ts` refuses to boot if any *safety-critical* pin (HLT SSR, INLET valve, HLT level inputs, boil PWM) is unmapped — unless `MOCK_HARDWARE=1`. Map real BCM numbers there first.
- **The parallel WAIT model is the whole point.** `src/domains/brewing/brew.ts` `steps[]` mirrors `brew.c`'s `BrewSteps[]` (line ~2056) including the `wait` flag (= `ucWait`): `wait:false` fires the step and advances immediately (runs in parallel); `wait:true` first awaits *all* previously-launched steps. This overlap (HLT reheating the next sparge water during the current mash/sparge) is what makes the brew ~3 h faster than naive sequential execution. Don't "simplify" it to sequential `await`s.

### Known gaps / safety notes (as of branch `…WAjwk`)
These are documented so future work doesn't assume the port is complete:
- **No continuous boil-kettle level monitor.** `boil.ts` only checks `BOIL_LEVEL` at the moment `setDuty()` is called; during a long boil the element holds duty with no re-check. The HLT has a background `levelMonitor`; the boil does not — dry-fire risk.
- **`unhandledRejection` triggers a full `shutdown()`** (`index.ts`), while the code intentionally rejects fire-and-forget promises (diagnostics `hlt.startHeating()`, `ensureOneInFlight` supersede). An un-awaited rejection can abort the whole machine. Fail-safe, but an availability footgun.
- **Watchdog escalation is aggressive:** ~5 consecutive unstable HLT temp reads (~1 s) calls `watchdog.fail()`, which stops kicking and lets the SoC watchdog reboot the Pi mid-brew. A transient DS18B20 glitch can kill a multi-hour brew.
- **Valve setters are non-idempotent.** `valves.open/close` always write GPIO, emit a store `change`, log, and fire `onOpen/onClose` hooks — even when already in that state. The HLT idle loop calls them every ~200 ms, so edge hooks (`flow.setMeasuring`) fire on non-edges and WebSocket/log traffic is spammy. Suppress the emit/log/hook when state is unchanged (but keep re-asserting the physical line if that's intended).
- **HLT command state** (`cmd`/`cmdParams` module globals) has no mutual exclusion; single-command-in-flight correctness depends entirely on the brew step WAIT ordering. Diagnostics actions during a brew can interleave commands.

## Git workflow notes
- The repo currently commits build output (`Debug/`, `Release/`, `*.axf`, `*.bin`, `*.map`) and IDE metadata (`.idea/`, `.settings/`, `.cproject`, `.project`). These are generated; avoid hand-editing them, and don't rely on them being current.
