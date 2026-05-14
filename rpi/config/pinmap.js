// Pin map for the Raspberry Pi 5 port of Brew Machine MkIV.
//
// Every value is the BCM pin number. Set to `null` for signals you haven't
// wired yet — the HAL will refuse to claim a null pin (unless MOCK_HARDWARE=1).
//
// The original STM32 pin assignments are noted in comments so you can match
// the wiring loom up. Many were on a PCF8574 I2C IO-expander — on the Pi
// you can either keep using the expander (`{ expander: 'U', bit: 6 }`) or
// move the signal to a native GPIO by setting `bcm: <pin>` instead.

'use strict';

// Native GPIO outputs (relay-board style, active high)
module.exports.outputs = {
  // Valves (orig valves.c)
  HLT_VALVE:        { bcm: null },  // orig PB0
  MASH_VALVE:       { bcm: null },  // orig PB1
  INLET_VALVE:      { bcm: null },  // orig PB5
  CHILLER_VALVE:    { bcm: null },  // orig PC9

  // Pumps
  MASH_PUMP:        { bcm: null },  // orig PC3  (mash_pump.c)
  CHILLER_PUMP:    { bcm: null },  // orig PC2  (chiller_pump.c)

  // Mill
  MILL:             { bcm: null },  // orig PE6  (mill.c)

  // HLT heating SSR (digital on/off, used in fill+heat / diag mode)
  HLT_SSR:          { bcm: null },  // orig PC11 (hlt.c)

  // Crane H-bridge — orig PCF8574 PORTU pins 6 & 7. Move to native GPIO or
  // keep on expander by replacing `bcm` with `{ expander: 'U', bit: 6 }`.
  CRANE_UP:         { bcm: null },  // orig PORTU bit 6
  CRANE_DOWN:       { bcm: null },  // orig PORTU bit 7

  // Stir motor — orig PCF8574 PORTU pin 0
  STIR:             { bcm: null },  // orig PORTU bit 0

  // Hop dropper drive — orig PCF8574 PORTU pin 5
  HOP_DROPPER:      { bcm: null },  // orig PORTU bit 5

  // Boil valve H-bridge — orig PCF8574 PORTU pins 1 & 2
  BOIL_VALVE_OPEN:  { bcm: null },  // orig PORTU bit 1
  BOIL_VALVE_CLOSE: { bcm: null },  // orig PORTU bit 2
};

// Native GPIO inputs (active low with pull-up unless noted)
module.exports.inputs = {
  HLT_LEVEL_MID:        { bcm: null, pull: 'up' }, // orig PA4
  HLT_LEVEL_HIGH:       { bcm: null, pull: 'up' }, // orig PA0
  BOIL_LEVEL:           { bcm: null, pull: 'up' }, // orig PC12 (override returns HIGH in original)
  HOP_DROPPER_LIMIT:    { bcm: null, pull: 'up' }, // orig PA11
  BOIL_VALVE_CLOSED:    { bcm: null, pull: 'up' }, // orig PE3
  BOIL_VALVE_OPENED:    { bcm: null, pull: 'up' }, // orig PE2

  // Crane limit switches — orig read via PCF8574 PORTV pin 1 (upper),
  // pin 2 (lower). Move to native GPIO or set `expander: 'V', bit: 0/1`.
  CRANE_UPPER_LIMIT:    { bcm: null, pull: 'up' },
  CRANE_LOWER_LIMIT:    { bcm: null, pull: 'up' },
};

// Hardware PWM channel for the boil-element SSR. Pi 5 hardware PWM lives on
// GPIO 12, 13, 18, 19. Requires `dtoverlay=pwm-2chan` in /boot/firmware/config.txt.
module.exports.pwm = {
  BOIL_SSR: {
    chip: 0,         // /sys/class/pwm/pwmchip0
    channel: 0,      // pwm0 -> GPIO12 with the default overlay
    bcm: 12,         // informational only
    periodHz: 1,     // boil element does very slow PWM (1 Hz period)
  },
};

// Flow sensor — falling-edge pulse counter (orig EXTI4 on PE4).
module.exports.pulseInputs = {
  BOIL_FLOW: { bcm: null, edge: 'falling', debounceUs: 100 },
};

// 1-Wire bus for DS18B20 temperature sensors.
// Default Pi 1-Wire is GPIO4. Enable with `dtoverlay=w1-gpio` in
// /boot/firmware/config.txt (or `sudo raspi-config` -> Interface Options).
// ROM codes from the original C source. Replace with your own sensors'
// ROMs once you've identified them via `ls /sys/bus/w1/devices`.
module.exports.oneWire = {
  busPath: '/sys/bus/w1/devices',
  sensors: {
    HLT:  '28-c652b604000023', // orig HLT_TEMP_SENSOR
    MASH: '28-d7c6b50400006f', // orig MASH_TEMP_SENSOR
    // BOIL: '28-...',          // add additional sensors as needed
  },
};

// I2C bus for the PCF8574 IO expander (only used if any output/input above
// uses `expander: 'U'/'V'/...` instead of `bcm`).
module.exports.i2c = {
  busNumber: 1,    // Pi default I2C bus
};
