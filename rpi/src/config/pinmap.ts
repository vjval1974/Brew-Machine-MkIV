// Pin map for the Raspberry Pi 5 port of Brew Machine MkIV.
//
// See docs/wiring.md for the full wiring diagram, BoM, and safety-chain
// schematic. The BCM assignments below are the recommended layout that
// the diagram uses; change them if your wiring differs.
//
// Four pins remain `bcm: null` by design — they are SAFETY-CRITICAL
// signals (heater, inlet valve, level switches). The HAL refuses to
// boot on real hardware until you assign them, so a forgotten wire
// can't silently turn into "heater on, level unknown". See
// assertSafetyCriticalPinsMapped() in index.ts. To bench-test the UI
// without wiring, set MOCK_HARDWARE=1.

import type { PinMap } from '../types';

const outputs: PinMap['outputs'] = {
  // Discrete valves — open-collector relay drivers, active high.
  HLT_VALVE:        { bcm: 5  },  // orig PB0  | header pin 29
  MASH_VALVE:       { bcm: 6  },  // orig PB1  | header pin 31
  INLET_VALVE:      { bcm: null }, // orig PB5 | RECOMMENDED: BCM 16 (pin 36) — SAFETY: overflow risk if unmapped
  CHILLER_VALVE:    { bcm: 17 },  // orig PC9  | header pin 11

  // Pumps
  MASH_PUMP:        { bcm: 20 },  // orig PC3  | header pin 38
  CHILLER_PUMP:     { bcm: 21 },  // orig PC2  | header pin 40

  // Grain mill
  MILL:             { bcm: 22 },  // orig PE6  | header pin 15

  // HLT heater SSR (digital on/off — the boil SSR is hardware PWM,
  // see `pwm` below).
  HLT_SSR:          { bcm: null }, // orig PC11 | RECOMMENDED: BCM 23 (pin 16) — SAFETY: dry-fire risk if unmapped

  // Crane H-bridge
  CRANE_UP:         { bcm: 24 },  // header pin 18
  CRANE_DOWN:       { bcm: 25 },  // header pin 22

  // Stir motor
  STIR:             { bcm: 27 },  // header pin 13

  // Hop dropper drive
  HOP_DROPPER:      { bcm: 19 },  // header pin 35

  // Motorised boil valve H-bridge
  BOIL_VALVE_OPEN:  { bcm: 2  },  // header pin 3
  BOIL_VALVE_CLOSE: { bcm: 3  },  // header pin 5

  // External hardware watchdog IC kick line (toggle, not write).
  // Wired to TPS3823 / MAX6369 / ATtiny WDI input. Its OK output is in
  // series with the mains contactor coil.
  WATCHDOG_KICK:    { bcm: 26 },  // header pin 37
};

const inputs: PinMap['inputs'] = {
  // HLT float-level switches (NC contacts to GND, internal pull-up).
  HLT_LEVEL_MID:        { bcm: null, pull: 'up' }, // orig PA4 | RECOMMENDED: BCM 7  (pin 26) — SAFETY: heater interlock
  HLT_LEVEL_HIGH:       { bcm: null, pull: 'up' }, // orig PA0 | RECOMMENDED: BCM 8  (pin 24) — SAFETY: overflow + interlock

  // Boil level (optional — original code hard-coded HIGH).
  BOIL_LEVEL:           { bcm: 15, pull: 'up' },  // orig PC12 | header pin 10

  // Hop dropper rotary limit (transitions on cup gap).
  HOP_DROPPER_LIMIT:    { bcm: 14, pull: 'up' },  // orig PA11 | header pin 8

  // Boil valve end-of-travel limits.
  BOIL_VALVE_CLOSED:    { bcm: 10, pull: 'up' },  // orig PE3  | header pin 19
  BOIL_VALVE_OPENED:    { bcm: 9,  pull: 'up' },  // orig PE2  | header pin 21

  // Crane end-of-travel limits.
  CRANE_UPPER_LIMIT:    { bcm: 11, pull: 'up' },  // header pin 23
  CRANE_LOWER_LIMIT:    { bcm: 13, pull: 'up' },  // header pin 33
};

const pwm: PinMap['pwm'] = {
  BOIL_SSR: {
    chip: 0,         // /sys/class/pwm/pwmchip0
    channel: 0,      // pwm0 -> GPIO12 with pwm-2chan overlay
    bcm: 12,         // header pin 32
    periodHz: 1,     // boil element uses very slow PWM (1 Hz period)
  },
};

const pulseInputs: PinMap['pulseInputs'] = {
  // Hall-effect flow sensor (YF-S201 or similar). Falling-edge events
  // counted in a worker thread. See platform/hal/workers/flow-worker.ts.
  BOIL_FLOW: { bcm: 18, edge: 'falling', debounceUs: 100 },  // header pin 12
};

const oneWire: PinMap['oneWire'] = {
  // Linux w1-gpio default bus is BCM 4 (header pin 7). Requires
  // `dtoverlay=w1-gpio` in /boot/firmware/config.txt + a 4.7 kΩ pull-up
  // to 3V3. The ROM codes below are pinmap defaults — assign the right
  // ones for your physical probes from the Diagnostics tab in the UI
  // (stored as overrides in data/onewire-overrides.json).
  busPath: '/sys/bus/w1/devices',
  sensors: {
    HLT:  '28-c652b604000023',
    MASH: '28-d7c6b50400006f',
  },
};

const i2c: PinMap['i2c'] = {
  busNumber: 1,
};

const pinmap: PinMap = { outputs, inputs, pwm, pulseInputs, oneWire, i2c };

export default pinmap;
export { outputs, inputs, pwm, pulseInputs, oneWire, i2c };
