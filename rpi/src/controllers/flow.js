'use strict';

// Flow sensor — port of Flow1.c. The original counted GPIO falling-edge
// interrupts and converted them to litres every 500ms using a two-threshold
// scheme (low rate × 0.0043 L/pulse, high rate × 0.0052 L/pulse, anomalies
// pinned to 15 pulses × high coefficient).

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');

const LITRES_PER_PULSE_LOW  = 0.0043;
const LITRES_PER_PULSE_HIGH = 0.0052;
const LOWER_THRESH = 13;   // pulses per 500 ms window
const UPPER_THRESH = 30;
const TICK_MS = 500;

let pulses = 0;
let lastPulses = 0;
let litres = 0.0;
let measuring = false;
let runner = null;

function _onPulse() { pulses++; }

function init() {
  const cfg = pinmap.pulseInputs.BOIL_FLOW;
  if (cfg.bcm === null) {
    log.warn('Flow: BOIL_FLOW BCM is null — pulse counting disabled');
    return;
  }
  gpio.acquirePulseInput('BOIL_FLOW', cfg.bcm, { edge: cfg.edge, onPulse: _onPulse });
  store.set('flow', { boilLitres: 0, flowing: false, measuring: false });
  log.info('Flow sensor initialised (BOIL_FLOW)');
}

async function _loop() {
  while (true) {
    if (measuring) {
      const delta = pulses - lastPulses;
      lastPulses = pulses;
      if (delta <= LOWER_THRESH) {
        litres += delta * LITRES_PER_PULSE_LOW;
      } else if (delta <= UPPER_THRESH) {
        litres += delta * LITRES_PER_PULSE_HIGH;
      } else {
        // Noise spike (e.g. mill running). Use an average.
        litres += 15 * LITRES_PER_PULSE_HIGH;
      }
      if (litres < 0.01 || litres > 50000) litres = 0;
      const flowing = delta > 0 && delta <= UPPER_THRESH;
      store.set('flow', { boilLitres: +litres.toFixed(3), flowing, measuring: true });
    } else {
      if (store.get('flow').flowing) {
        store.set('flow', { ...store.get('flow'), flowing: false });
      }
    }
    await new Promise((r) => setTimeout(r, TICK_MS));
  }
}

function start() {
  if (runner) return;
  runner = _loop().catch((err) => log.error(`Flow loop crashed: ${err.message}`));
}

function reset() {
  pulses = 0;
  lastPulses = 0;
  litres = 0;
  store.set('flow', { ...store.get('flow'), boilLitres: 0 });
  log.info('Flow: reset');
}

function setMeasuring(on) {
  measuring = !!on;
  store.set('flow', { ...store.get('flow'), measuring });
}

function getLitres()    { return litres; }
function isMeasuring()  { return measuring; }
function isFlowing()    { return store.get('flow').flowing; }

module.exports = {
  init, start, reset, setMeasuring,
  getLitres, isMeasuring, isFlowing,
  _injectPulses: (n) => { pulses += n; },     // test helper for mock mode
};
