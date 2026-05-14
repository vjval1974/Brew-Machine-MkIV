'use strict';

// Temperature poller — reads HLT + MASH DS18B20s every second and publishes
// to the store. Replaces the FreeRTOS `vTaskDS1820Convert` task that did
// the same on the STM32. The conversion + ROM-search bit-banging is
// delegated to the kernel `w1-therm` driver via hal/onewire.js.

const log = require('../util/logger');
const onewire = require('../hal/onewire');
const store = require('../state/store');

let runner = null;

async function _loop() {
  while (true) {
    try {
      const hlt  = await onewire.readSensor('HLT');
      const mash = await onewire.readSensor('MASH');
      store.set('temps', { HLT: hlt, MASH: mash });
    } catch (err) {
      log.warn(`Temp poll: ${err.message}`);
    }
    await new Promise((r) => setTimeout(r, 1000));
  }
}

function start() {
  if (runner) return;
  runner = _loop().catch((err) => log.error(`Temp loop crashed: ${err.message}`));
}

function get(name) { return store.get('temps')[name]; }

module.exports = { start, get };
