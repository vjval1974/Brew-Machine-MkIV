# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

## Git workflow notes
- The repo currently commits build output (`Debug/`, `Release/`, `*.axf`, `*.bin`, `*.map`) and IDE metadata (`.idea/`, `.settings/`, `.cproject`, `.project`). These are generated; avoid hand-editing them, and don't rely on them being current.
