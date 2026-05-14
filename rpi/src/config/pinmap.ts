// Pin map for the Raspberry Pi 5 port of Brew Machine MkIV.
//
// Every value is the BCM pin number. Set to `null` for signals you haven't
// wired yet — the HAL will refuse to claim a null pin (unless MOCK_HARDWARE=1).
//
// The original STM32 pin assignments are noted in comments so you can match
// the wiring loom up. Signals that were on the PCF8574 I2C IO-expander in
// the original can be moved to native GPIO here, or kept on the expander
// (see src/hal/i2c.ts).

import type { PinMap } from '../types';

const outputs: PinMap['outputs'] = {
  // Valves (orig valves.c)
  HLT_VALVE:        { bcm: null },  // orig PB0
  MASH_VALVE:       { bcm: null },  // orig PB1
  INLET_VALVE:      { bcm: null },  // orig PB5
  CHILLER_VALVE:    { bcm: null },  // orig PC9

  // Pumps
  MASH_PUMP:        { bcm: null },  // orig PC3
  CHILLER_PUMP:     { bcm: null },  // orig PC2

  // Mill
  MILL:             { bcm: null },  // orig PE6

  // HLT heating SSR (digital on/off)
  HLT_SSR:          { bcm: null },  // orig PC11

  // Crane H-bridge — orig PCF8574 PORTU bits 6 & 7
  CRANE_UP:         { bcm: null },
  CRANE_DOWN:       { bcm: null },

  // Stir motor — orig PCF8574 PORTU bit 0
  STIR:             { bcm: null },

  // Hop dropper drive — orig PCF8574 PORTU bit 5
  HOP_DROPPER:      { bcm: null },

  // Boil valve H-bridge — orig PCF8574 PORTU bits 1 & 2
  BOIL_VALVE_OPEN:  { bcm: null },
  BOIL_VALVE_CLOSE: { bcm: null },
};

const inputs: PinMap['inputs'] = {
  HLT_LEVEL_MID:        { bcm: null, pull: 'up' }, // orig PA4
  HLT_LEVEL_HIGH:       { bcm: null, pull: 'up' }, // orig PA0
  BOIL_LEVEL:           { bcm: null, pull: 'up' }, // orig PC12
  HOP_DROPPER_LIMIT:    { bcm: null, pull: 'up' }, // orig PA11
  BOIL_VALVE_CLOSED:    { bcm: null, pull: 'up' }, // orig PE3
  BOIL_VALVE_OPENED:    { bcm: null, pull: 'up' }, // orig PE2
  CRANE_UPPER_LIMIT:    { bcm: null, pull: 'up' },
  CRANE_LOWER_LIMIT:    { bcm: null, pull: 'up' },
};

const pwm: PinMap['pwm'] = {
  BOIL_SSR: {
    chip: 0,         // /sys/class/pwm/pwmchip0
    channel: 0,      // pwm0 -> GPIO12 with pwm-2chan overlay
    bcm: 12,
    periodHz: 1,     // boil element uses very slow PWM (1 Hz period)
  },
};

const pulseInputs: PinMap['pulseInputs'] = {
  BOIL_FLOW: { bcm: null, edge: 'falling', debounceUs: 100 },
};

const oneWire: PinMap['oneWire'] = {
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
