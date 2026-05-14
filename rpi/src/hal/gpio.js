'use strict';

// GPIO HAL — wraps node-libgpiod for Pi 5 (and earlier). Falls back to a
// mock backend if MOCK_HARDWARE=1 or libgpiod isn't installed.
//
// Original C used the STM32 std-periph driver:
//   GPIO_Init(...); GPIO_WriteBit(...); GPIO_ReadInputDataBit(...);
// Replaced here with explicit acquire / setValue / getValue calls.

const log = require('../util/logger');
const bus = require('../util/emitter');

const MOCK = process.env.MOCK_HARDWARE === '1';

let libgpiod = null;
if (!MOCK) {
  try {
    libgpiod = require('node-libgpiod');
  } catch (err) {
    log.warn(`node-libgpiod not available (${err.message}). Falling back to mock GPIO.`);
  }
}

// Pi 5 uses /dev/gpiochip4 (RP1 bank 0). Pi 4 and earlier use /dev/gpiochip0.
// Detect by looking for gpiochip4 first.
function detectChip() {
  if (!libgpiod) return null;
  const fs = require('fs');
  for (const path of ['/dev/gpiochip4', '/dev/gpiochip0']) {
    try {
      fs.accessSync(path);
      return path;
    } catch (_) { /* keep looking */ }
  }
  return '/dev/gpiochip0';
}

class RealGpioBackend {
  constructor() {
    this.chipPath = detectChip();
    log.info(`GPIO backend: libgpiod on ${this.chipPath}`);
    this.chip = new libgpiod.Chip(this.chipPath);
    this.lines = new Map();          // bcm -> Line
    this.values = new Map();         // bcm -> last known value
    this.watchers = new Map();       // bcm -> array of callbacks
  }

  acquireOutput(name, bcm, initial = 0) {
    if (bcm === null || bcm === undefined) {
      throw new Error(`GPIO ${name}: pinmap entry is null — assign a BCM pin in config/pinmap.js`);
    }
    const line = new libgpiod.Line(this.chip, bcm);
    line.requestOutputMode(initial, `brew-${name}`);
    this.lines.set(bcm, line);
    this.values.set(bcm, initial);
  }

  acquireInput(name, bcm, { pull = 'up' } = {}) {
    if (bcm === null || bcm === undefined) {
      throw new Error(`GPIO ${name}: pinmap entry is null — assign a BCM pin in config/pinmap.js`);
    }
    const line = new libgpiod.Line(this.chip, bcm);
    const flags = pull === 'up' ? libgpiod.LINE_REQ_FLAG_BIAS_PULL_UP
                : pull === 'down' ? libgpiod.LINE_REQ_FLAG_BIAS_PULL_DOWN
                : 0;
    line.requestInputMode(`brew-${name}`, flags);
    this.lines.set(bcm, line);
  }

  // Edge-triggered input for the flow pulse counter.
  acquirePulseInput(name, bcm, { edge = 'falling', onPulse } = {}) {
    if (bcm === null || bcm === undefined) {
      throw new Error(`GPIO ${name}: pinmap entry is null`);
    }
    const line = new libgpiod.Line(this.chip, bcm);
    const flag = edge === 'rising' ? libgpiod.LINE_REQ_EV_RISING_EDGE
              : edge === 'both'   ? libgpiod.LINE_REQ_EV_BOTH_EDGES
              :                     libgpiod.LINE_REQ_EV_FALLING_EDGE;
    line.requestEventMode(`brew-${name}`, flag);
    this.lines.set(bcm, line);
    // poll for events in a tight async loop
    const poll = async () => {
      while (this.lines.has(bcm)) {
        try {
          const ev = await line.eventWait(BigInt(500_000_000)); // 500 ms timeout
          if (ev) {
            line.eventRead();
            if (onPulse) onPulse();
          }
        } catch (err) {
          log.error(`Pulse poll ${name}: ${err.message}`);
          await new Promise((r) => setTimeout(r, 200));
        }
      }
    };
    poll();
  }

  setValue(bcm, value) {
    const line = this.lines.get(bcm);
    if (!line) throw new Error(`GPIO bcm=${bcm} not acquired`);
    line.setValue(value ? 1 : 0);
    this.values.set(bcm, value ? 1 : 0);
  }

  getValue(bcm) {
    const line = this.lines.get(bcm);
    if (!line) throw new Error(`GPIO bcm=${bcm} not acquired`);
    return line.getValue();
  }

  cleanup() {
    log.info('GPIO cleanup: driving all outputs low');
    for (const [bcm, line] of this.lines) {
      try {
        if (typeof line.setValue === 'function') line.setValue(0);
        if (typeof line.release === 'function') line.release();
      } catch (_) { /* ignore */ }
    }
    this.lines.clear();
    try { this.chip.close && this.chip.close(); } catch (_) {}
  }
}

class MockGpioBackend {
  constructor() {
    log.info('GPIO backend: MOCK');
    this.outputs = new Map();
    this.inputs = new Map();
  }
  acquireOutput(name, bcm, initial = 0) {
    if (bcm === null) {
      log.warn(`Mock GPIO: output "${name}" has null BCM — accepted in mock mode`);
    }
    this.outputs.set(name, { bcm, value: initial });
  }
  acquireInput(name, bcm /*, opts */) {
    this.inputs.set(name, { bcm, value: 1 });   // pull-up default
  }
  acquirePulseInput(/* name, bcm, opts */) {
    // mock: never fires
  }
  setValue(bcm, value) {
    for (const v of this.outputs.values()) {
      if (v.bcm === bcm) v.value = value ? 1 : 0;
    }
  }
  getValue(bcm) {
    for (const v of this.outputs.values()) if (v.bcm === bcm) return v.value;
    for (const v of this.inputs.values())  if (v.bcm === bcm) return v.value;
    return 0;
  }
  // Test helpers (used by REST endpoints in mock mode)
  _setInput(name, value) {
    const i = this.inputs.get(name);
    if (i) i.value = value ? 1 : 0;
  }
  cleanup() {}
}

const backend = (MOCK || !libgpiod) ? new MockGpioBackend() : new RealGpioBackend();

const acquiredOutputs = {}; // name -> bcm
const acquiredInputs  = {}; // name -> bcm

function acquireOutput(name, bcm, initial = 0) {
  backend.acquireOutput(name, bcm, initial);
  acquiredOutputs[name] = bcm;
}

function acquireInput(name, bcm, opts) {
  backend.acquireInput(name, bcm, opts);
  acquiredInputs[name] = bcm;
}

function acquirePulseInput(name, bcm, opts) {
  backend.acquirePulseInput(name, bcm, opts);
}

function writeOutput(name, value) {
  const bcm = acquiredOutputs[name];
  if (bcm === undefined) throw new Error(`Output ${name} not acquired`);
  backend.setValue(bcm, value);
  bus.emit('gpio:output', { name, value: value ? 1 : 0 });
}

function readInput(name) {
  const bcm = acquiredInputs[name];
  if (bcm === undefined) throw new Error(`Input ${name} not acquired`);
  return backend.getValue(bcm);
}

function cleanup() {
  backend.cleanup();
}

// Mock-only — let the UI flip input states from the dashboard.
function _mockSetInput(name, value) {
  if (typeof backend._setInput === 'function') backend._setInput(name, value);
}

module.exports = {
  acquireOutput, acquireInput, acquirePulseInput,
  writeOutput, readInput,
  cleanup,
  isMock: () => MOCK || !libgpiod,
  _mockSetInput,
};
